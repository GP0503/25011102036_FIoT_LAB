/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F4 + BME280 (I2C1) + ESP8266 (USART1) -> MQTT
  *                   Broker : broker.emqx.io : 1883 (plain TCP, MQTT 3.1.1)
  *                   Debug  : USART2 -> PuTTY (115200 8N1)
  ******************************************************************************
  *
  *  How it works
  *  ------------
  *  The STM32 builds the MQTT packets itself (CONNECT, PUBLISH, PINGREQ)
  *  and sends them through an ESP8266 TCP connection:
  *
  *      AT+CIPSTART="TCP","broker.emqx.io",1883
  *      AT+CIPSEND=<n>   ->  ">"  ->  <MQTT packet bytes>  ->  "SEND OK"
  *
  *  This works with every ESP8266 AT firmware (the AT+MQTT... commands
  *  only exist in ESP-AT 2.x, which most ESP-01 modules cannot run).
  *
  *  Every MQTT_PUBLISH_INTERVAL_MS the latest BME280 reading is published
  *  to MQTT_TOPIC as JSON:
  *
  *      {"temperature":24.90,"pressure":1012.97,"humidity":59.14}
  *
  *  If the connection drops, the code reconnects automatically.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include "bme280.h"
#include <stdio.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/* ----------------------------- Wi-Fi ----------------------------- */

#define WIFI_SSID                  "SNUC"
#define WIFI_PASSWORD              "snu12345"

/* ----------------------------- MQTT ------------------------------ */

#define MQTT_BROKER                "broker.emqx.io"
#define MQTT_PORT                  1883

/* turns MQTT_PORT into the text "1883" (no printf needed) */
#define MQTT_STR_(x)               #x
#define MQTT_STR(x)                MQTT_STR_(x)

/* broker.emqx.io is PUBLIC: anyone subscribed to this topic sees the
   data. Change "node1" to something unique to you. */
#define MQTT_TOPIC                 "snuc-iot/stm32-bme280/node1"

/* Client ID = MQTT_CLIENT_PREFIX + 8 hex digits of the STM32 unique ID,
   so it never collides with another device on the public broker. */
#define MQTT_CLIENT_PREFIX         "stm32-bme280-"

/* Leave empty for broker.emqx.io (no login needed) */
#define MQTT_USERNAME              ""
#define MQTT_PASSWORD              ""

#define MQTT_KEEPALIVE_S           60U
#define MQTT_PUBLISH_INTERVAL_MS   5000U
#define MQTT_RETRY_INTERVAL_MS     5000U

/* After this many failed broker connects in a row, redo Wi-Fi join */
#define MQTT_MAX_CONNECT_FAILS     5U

/* ---------------------------- ESP8266 ---------------------------- */

/* 1 = show ESP8266 replies on PuTTY as "  [ESP] ..." during setup */
#define ESP_DEBUG_ECHO             1

/* USART1 receive ring buffer size - MUST be a power of 2 */
#define ESP_RX_BUF_SIZE            2048U

/* Private typedef -----------------------------------------------------------*/

typedef enum
{
    IPD_IDLE = 0,
    IPD_HEADER,
    IPD_PAYLOAD
} IPD_State;

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* BME280 device */
static struct bme280_dev bme280_dev;

/* Sensor readings */
static struct bme280_data comp_data;

/* Sensor configuration */
static struct bme280_settings settings;

/* I2C address - SDO connected to GND */
static uint8_t bme280_i2c_address =
    BME280_I2C_ADDR_PRIM;

/* API return status */
static int8_t rslt;

/* UART output buffer */
static char uart_buffer[128];

/* 1 after at least one good BME280 reading */
static uint8_t bme_data_valid = 0U;


/* ------------------ USART1 (ESP8266) interrupt RX ------------------ */

static volatile uint8_t  esp_rx_buf[ESP_RX_BUF_SIZE];
static volatile uint16_t esp_rx_head = 0U;      /* written by the ISR only */
static volatile uint16_t esp_rx_tail = 0U;      /* written by main only    */
static volatile uint32_t esp_rx_overflow = 0U;

/* ------------------ ESP8266 reply / line handling ------------------ */

static char     esp_reply[512];
static uint16_t esp_reply_len = 0U;

static char     esp_line[128];
static uint16_t esp_line_len = 0U;
static uint8_t  esp_dbg_mute = 0U;

/* ------------------ +IPD (data from broker) parser ----------------- */

static IPD_State ipd_state = IPD_IDLE;
static uint8_t   ipd_match = 0U;
static char      ipd_hdr[48];
static uint8_t   ipd_hdr_len = 0U;
static uint16_t  ipd_remaining = 0U;
static uint8_t   ipd_data[64];
static uint16_t  ipd_data_len = 0U;

/* ----------------------------- state ------------------------------- */

static uint8_t  esp_ready = 0U;            /* Wi-Fi joined              */
static uint8_t  esp_tcp_open = 0U;         /* TCP link to broker is up  */
static uint8_t  mqtt_connected = 0U;       /* CONNACK 0 received        */
static uint8_t  mqtt_connack_received = 0U;
static uint8_t  mqtt_connack_rc = 0xFFU;
static uint8_t  mqtt_connect_fails = 0U;

static uint32_t mqtt_publish_count = 0U;
static uint32_t mqtt_last_tx = 0U;
static uint32_t mqtt_last_publish = 0U;
static uint32_t mqtt_last_retry = 0U;

static char     mqtt_client_id[32];
static uint8_t  mqtt_packet[256];


/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);


/* BME280 functions */

static BME280_INTF_RET_TYPE user_i2c_read(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr
);

static BME280_INTF_RET_TYPE user_i2c_write(
    uint8_t reg_addr,
    const uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr
);

static void user_delay_us(
    uint32_t period,
    void *intf_ptr
);

static void UART_Print(
    const char *message
);


static void BME280_Init(void);

static void BME280_PrintData(void);


/* ESP8266 low level */

static void     ESP_RxStart(void);
static uint8_t  ESP_RxRead(uint8_t *c);
static void     ESP_SendRaw(const uint8_t *data, uint16_t len);
static void     ESP_SendString(const char *s);

static uint16_t ESP_Pump(void);
static void     ESP_ReplyReset(void);
static void     ESP_ReplyAppend(char c);
static void     ESP_LineChar(char c);
static void     ESP_OnLine(const char *line);

static int      ESP_WaitFor(const char *ok, const char *fail1,
                            const char *fail2, uint32_t timeout_ms);
static int      ESP_Command(const char *cmd, const char *ok,
                            uint32_t timeout_ms);
static int      ESP_Handshake(uint8_t attempts);
static int      ESP_SendTCP(const uint8_t *data, uint16_t len);
static void     ESP_PrintIP(void);
static void     ESP_PrintCWJAPError(void);

static uint8_t  IPD_Feed(uint8_t c);
static uint8_t  IPD_ParseHeader(void);

static int      ESP8266_Init(void);


/* MQTT */

static void     MQTT_MakeClientId(void);
static uint16_t MQTT_EncodeLength(uint8_t *buf, uint32_t len);
static uint16_t MQTT_WriteString(uint8_t *buf, const char *s);
static uint16_t MQTT_BuildConnect(uint8_t *buf, uint16_t size);
static uint16_t MQTT_BuildPublish(uint8_t *buf, uint16_t size,
                                  const char *topic,
                                  const char *payload, uint16_t payload_len);
static void     MQTT_HandleIncoming(const uint8_t *data, uint16_t len);
static int      MQTT_Connect(void);
static void     MQTT_Publish(void);
static void     MQTT_Ping(void);
static void     MQTT_Process(void);

/* USART1 interrupt handler */
void USART1_IRQHandler(void);


/* ========================================================================== */
/* MAIN                                                                       */
/* ========================================================================== */

int main(void)
{
    /* Reset of peripherals, Initializes Flash and Systick */
    HAL_Init();

    /* Configure system clock */
    SystemClock_Config();

    /* Initialize configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* Start interrupt receive on USART1 (settings unchanged) */
    ESP_RxStart();


    /* ---------------------------------------------------------------------- */
    /* START                                                                  */
    /* ---------------------------------------------------------------------- */

    UART_Print("\r\n");
    UART_Print("=================================\r\n");
    UART_Print("STM32 BME280 MQTT PROJECT\r\n");
    UART_Print("=================================\r\n");

    UART_Print("Starting sensor initialization...\r\n");


    /* ---------------------------------------------------------------------- */
    /* CHECK BME280                                                           */
    /* ---------------------------------------------------------------------- */

    if (HAL_I2C_IsDeviceReady(
            &hi2c1,
            (uint16_t)(bme280_i2c_address << 1),
            3,
            100U) != HAL_OK)
    {
        UART_Print(
            "ERROR: BME280 I2C device not responding\r\n"
        );

        Error_Handler();
    }


    /* ---------------------------------------------------------------------- */
    /* INITIALIZE BME280                                                      */
    /* ---------------------------------------------------------------------- */

    BME280_Init();


    /* Allow first measurement */
    HAL_Delay(1100U);


    /* ---------------------------------------------------------------------- */
    /* INITIALIZE ESP8266 + CONNECT TO MQTT BROKER                            */
    /* ---------------------------------------------------------------------- */

    MQTT_MakeClientId();

    if (ESP8266_Init() != 0)
    {
        (void)MQTT_Connect();
    }

    mqtt_last_retry = HAL_GetTick();


    /* ---------------------------------------------------------------------- */
    /* MAIN LOOP                                                              */
    /* ---------------------------------------------------------------------- */

    while (1)
    {
        /* Read BME280 sensor data */

        rslt = bme280_get_sensor_data(
            BME280_ALL,
            &comp_data,
            &bme280_dev
        );


        if (rslt == BME280_OK)
        {
            bme_data_valid = 1U;

            /* Display sensor data on PuTTY */

            BME280_PrintData();
        }
        else
        {
            int len = snprintf(
                uart_buffer,
                sizeof(uart_buffer),
                "ERROR: BME280 read failed: %d\r\n",
                (int)rslt
            );

            if ((len > 0) &&
                ((size_t)len < sizeof(uart_buffer)))
            {
                HAL_UART_Transmit(
                    &huart2,
                    (uint8_t *)uart_buffer,
                    (uint16_t)len,
                    1000U
                );
            }
        }


        /* Publish / keep-alive / reconnect */

        MQTT_Process();


        /* Wait for next measurement
           (USART1 keeps receiving in the interrupt meanwhile) */

        HAL_Delay(1100U);
    }
}


/* ========================================================================== */
/* SYSTEM CLOCK                                                               */
/* ========================================================================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE2
    );


    /* HSI + PLL */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSI;

    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 84;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;


    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }


    /* CPU / AHB / APB clocks */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;


    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ========================================================================== */
/* I2C1                                                                       */
/* ========================================================================== */

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;

    hi2c1.Init.ClockSpeed = 100000;

    hi2c1.Init.DutyCycle =
        I2C_DUTYCYCLE_2;

    hi2c1.Init.OwnAddress1 = 0;

    hi2c1.Init.AddressingMode =
        I2C_ADDRESSINGMODE_7BIT;

    hi2c1.Init.DualAddressMode =
        I2C_DUALADDRESS_DISABLE;

    hi2c1.Init.OwnAddress2 = 0;

    hi2c1.Init.GeneralCallMode =
        I2C_GENERALCALL_DISABLE;

    hi2c1.Init.NoStretchMode =
        I2C_NOSTRETCH_DISABLE;


    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ========================================================================== */
/* USART1                                                                     */
/* ESP8266                                                                    */
/* ========================================================================== */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;


    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ========================================================================== */
/* USART2                                                                     */
/* PuTTY / DEBUG                                                              */
/* ========================================================================== */

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;

    huart2.Init.BaudRate = 115200;

    huart2.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart2.Init.StopBits =
        UART_STOPBITS_1;

    huart2.Init.Parity =
        UART_PARITY_NONE;

    huart2.Init.Mode =
        UART_MODE_TX_RX;

    huart2.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart2.Init.OverSampling =
        UART_OVERSAMPLING_16;


    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ========================================================================== */
/* GPIO                                                                       */
/* ========================================================================== */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}


/* ========================================================================== */
/* BME280 I2C READ                                                           */
/* ========================================================================== */

static BME280_INTF_RET_TYPE user_i2c_read(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr)
{
    uint8_t dev_addr;


    if ((intf_ptr == NULL) ||
        (reg_data == NULL) ||
        (len == 0U) ||
        (len > UINT16_MAX))
    {
        return (BME280_INTF_RET_TYPE)-1;
    }


    dev_addr =
        *(uint8_t *)intf_ptr;


    if (HAL_I2C_Mem_Read(
            &hi2c1,
            (uint16_t)(dev_addr << 1),
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            reg_data,
            (uint16_t)len,
            100U) == HAL_OK)
    {
        return BME280_INTF_RET_SUCCESS;
    }


    return (BME280_INTF_RET_TYPE)-1;
}


/* ========================================================================== */
/* BME280 I2C WRITE                                                          */
/* ========================================================================== */

static BME280_INTF_RET_TYPE user_i2c_write(
    uint8_t reg_addr,
    const uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr)
{
    uint8_t dev_addr;


    if ((intf_ptr == NULL) ||
        (reg_data == NULL) ||
        (len == 0U) ||
        (len > UINT16_MAX))
    {
        return (BME280_INTF_RET_TYPE)-1;
    }


    dev_addr =
        *(uint8_t *)intf_ptr;


    if (HAL_I2C_Mem_Write(
            &hi2c1,
            (uint16_t)(dev_addr << 1),
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            (uint8_t *)reg_data,
            (uint16_t)len,
            100U) == HAL_OK)
    {
        return BME280_INTF_RET_SUCCESS;
    }


    return (BME280_INTF_RET_TYPE)-1;
}


/* ========================================================================== */
/* BME280 DELAY                                                              */
/* ========================================================================== */

static void user_delay_us(
    uint32_t period,
    void *intf_ptr)
{
    (void)intf_ptr;


    if (period > 0U)
    {
        HAL_Delay(
            (period + 999U) / 1000U
        );
    }
}


/* ========================================================================== */
/* UART2 PRINT                                                               */
/* ========================================================================== */

static void UART_Print(
    const char *message)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)message,
        (uint16_t)strlen(message),
        1000U
    );
}


/* ========================================================================== */
/* BME280 INITIALIZATION                                                     */
/* ========================================================================== */

static void BME280_Init(void)
{
    memset(
        &bme280_dev,
        0,
        sizeof(bme280_dev)
    );


    /* Configure Bosch driver */

    bme280_dev.intf_ptr =
        &bme280_i2c_address;

    bme280_dev.intf =
        BME280_I2C_INTF;

    bme280_dev.read =
        user_i2c_read;

    bme280_dev.write =
        user_i2c_write;

    bme280_dev.delay_us =
        user_delay_us;


    /* Initialize BME280 */

    rslt =
        bme280_init(&bme280_dev);


    if (rslt != BME280_OK)
    {
        UART_Print(
            "ERROR: BME280 initialization failed\r\n"
        );

        Error_Handler();
    }


    if (bme280_dev.chip_id !=
        BME280_CHIP_ID)
    {
        UART_Print(
            "ERROR: Invalid sensor ID\r\n"
        );

        Error_Handler();
    }


    UART_Print(
        "BME280 initialization successful\r\n"
    );

    UART_Print(
        "Sensor ID verified: 0x60\r\n"
    );


    /* Oversampling */

    settings.osr_t =
        BME280_OVERSAMPLING_1X;

    settings.osr_p =
        BME280_OVERSAMPLING_1X;

    settings.osr_h =
        BME280_OVERSAMPLING_1X;


    /* Filter OFF */

    settings.filter =
        BME280_FILTER_COEFF_OFF;


    /* Standby 1000 ms */

    settings.standby_time =
        BME280_STANDBY_TIME_1000_MS;


    /* Apply settings */

    rslt =
        bme280_set_sensor_settings(
            BME280_SEL_OSR_PRESS |
            BME280_SEL_OSR_TEMP |
            BME280_SEL_OSR_HUM |
            BME280_SEL_FILTER |
            BME280_SEL_STANDBY,

            &settings,

            &bme280_dev
        );


    if (rslt != BME280_OK)
    {
        UART_Print(
            "ERROR: Sensor configuration failed\r\n"
        );

        Error_Handler();
    }


    /* Normal mode */

    rslt =
        bme280_set_sensor_mode(
            BME280_POWERMODE_NORMAL,
            &bme280_dev
        );


    if (rslt != BME280_OK)
    {
        UART_Print(
            "ERROR: Sensor mode configuration failed\r\n"
        );

        Error_Handler();
    }


    UART_Print(
        "BME280 configured successfully\r\n"
    );
}


/* ========================================================================== */
/* BME280 DISPLAY                                                             */
/* ========================================================================== */

static void BME280_PrintData(void)
{
    int32_t temp_centi;

    uint32_t temp_abs;

    uint32_t pressure_pa;

    uint32_t humidity_centi;

    int len;


    /* Temperature */

    temp_centi =
        (int32_t)(comp_data.temperature * 100.0);


    temp_abs =
        (temp_centi < 0) ?
        (uint32_t)(-temp_centi) :
        (uint32_t)temp_centi;


    /* Pressure */

    pressure_pa =
        (uint32_t)(comp_data.pressure + 0.5);


    /* Humidity */

    humidity_centi =
        (uint32_t)(
            comp_data.humidity * 100.0 + 0.5
        );


    len = snprintf(
        uart_buffer,
        sizeof(uart_buffer),

        "Temperature: %s%lu.%02lu C\r\n"
        "Pressure: %lu.%02lu hPa\r\n"
        "Humidity: %lu.%02lu %%RH\r\n"
        "-----------------------------\r\n",

        (temp_centi < 0) ? "-" : "",

        (unsigned long)(temp_abs / 100U),
        (unsigned long)(temp_abs % 100U),

        (unsigned long)(pressure_pa / 100U),
        (unsigned long)(pressure_pa % 100U),

        (unsigned long)(humidity_centi / 100U),
        (unsigned long)(humidity_centi % 100U)
    );


    if ((len > 0) &&
        ((size_t)len < sizeof(uart_buffer)))
    {
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)uart_buffer,
            (uint16_t)len,
            1000U
        );
    }
}


/* ========================================================================== */
/* USART1 INTERRUPT  (ESP8266 -> STM32)                                       */
/*                                                                            */
/* Every received byte goes into esp_rx_buf[]. Reading SR then DR clears      */
/* RXNE / ORE / FE / NE on STM32F4, so reception never locks up.              */
/*                                                                            */
/* If Keil reports "USART1_IRQHandler multiply defined", delete the           */
/* USART1_IRQHandler() function in stm32f4xx_it.c.                            */
/* ========================================================================== */

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE |
               USART_SR_FE   | USART_SR_NE  | USART_SR_PE)) != 0U)
    {
        uint8_t  byte = (uint8_t)(USART1->DR & 0xFFU);
        uint16_t head = esp_rx_head;
        uint16_t next = (uint16_t)((head + 1U) & (ESP_RX_BUF_SIZE - 1U));

        if (next != esp_rx_tail)
        {
            esp_rx_buf[head] = byte;
            esp_rx_head = next;
        }
        else
        {
            esp_rx_overflow++;
        }
    }
}


/* ========================================================================== */
/* ESP8266 LOW LEVEL                                                          */
/* ========================================================================== */

/* Enable the USART1 receive interrupt (baud/format are not touched) */
static void ESP_RxStart(void)
{
    HAL_NVIC_DisableIRQ(USART1_IRQn);

    esp_rx_head = 0U;
    esp_rx_tail = 0U;

    /* Read SR then DR: drop any stale byte, clear ORE/FE/NE */
    __HAL_UART_CLEAR_OREFLAG(&huart1);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

    HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}


static uint8_t ESP_RxRead(uint8_t *c)
{
    uint16_t tail = esp_rx_tail;

    if (tail == esp_rx_head)
    {
        return 0U;
    }

    *c = esp_rx_buf[tail];
    esp_rx_tail = (uint16_t)((tail + 1U) & (ESP_RX_BUF_SIZE - 1U));

    return 1U;
}


static void ESP_SendRaw(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 5000U);
}


static void ESP_SendString(const char *s)
{
    ESP_SendRaw((const uint8_t *)s, (uint16_t)strlen(s));
}


/* ========================================================================== */
/* ESP8266 REPLY HANDLING                                                     */
/* ========================================================================== */

static void ESP_ReplyReset(void)
{
    esp_reply_len = 0U;
    esp_reply[0] = '\0';
}


static void ESP_ReplyAppend(char c)
{
    if (c == '\0')
    {
        return;
    }

    if (esp_reply_len >= (uint16_t)(sizeof(esp_reply) - 1U))
    {
        /* Full: keep the newest half so matching continues */
        uint16_t keep = (uint16_t)(sizeof(esp_reply) / 2U);

        memmove(esp_reply, &esp_reply[esp_reply_len - keep], keep);
        esp_reply_len = keep;
    }

    esp_reply[esp_reply_len++] = c;
    esp_reply[esp_reply_len] = '\0';
}


/* Collect ESP8266 output into lines */
static void ESP_LineChar(char c)
{
    if (c == '\r')
    {
        return;
    }

    if (c == '\n')
    {
        if (esp_line_len > 0U)
        {
            esp_line[esp_line_len] = '\0';
            ESP_OnLine(esp_line);
            esp_line_len = 0U;
        }

        return;
    }

    if (((unsigned char)c < 32U) || ((unsigned char)c > 126U))
    {
        c = '.';
    }

    if (esp_line_len < (uint16_t)(sizeof(esp_line) - 1U))
    {
        esp_line[esp_line_len++] = c;
    }
}


/* One complete line from the ESP8266: track link state, print it */
static void ESP_OnLine(const char *line)
{
    if (strcmp(line, "CLOSED") == 0)
    {
        if (mqtt_connected != 0U)
        {
            UART_Print("MQTT: connection to broker closed\r\n");
        }

        esp_tcp_open   = 0U;
        mqtt_connected = 0U;
    }
    else if (strcmp(line, "WIFI DISCONNECT") == 0)
    {
        if (mqtt_connected != 0U)
        {
            UART_Print("MQTT: Wi-Fi disconnected\r\n");
        }

        esp_tcp_open   = 0U;
        mqtt_connected = 0U;
    }
    else
    {
        /* nothing to track */
    }

    if ((ESP_DEBUG_ECHO != 0) && (esp_dbg_mute == 0U))
    {
        UART_Print("  [ESP] ");
        UART_Print(line);
        UART_Print("\r\n");
    }
}


/*
 * Move received bytes through the +IPD parser.
 * Normal ESP8266 text goes into esp_reply and the line handler;
 * broker data (+IPD payload) goes to the MQTT handler.
 */
static uint16_t ESP_Pump(void)
{
    uint8_t  c;
    uint16_t added = 0U;

    while (ESP_RxRead(&c) != 0U)
    {
        if (IPD_Feed(c) == 0U)
        {
            ESP_ReplyAppend((char)c);
            ESP_LineChar((char)c);
            added++;
        }
    }

    return added;
}


/* Wait for `ok` (returns 1) or `fail1`/`fail2`/timeout (returns 0) */
static int ESP_WaitFor(const char *ok,
                       const char *fail1,
                       const char *fail2,
                       uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    for (;;)
    {
        (void)ESP_Pump();

        if ((ok != NULL) && (strstr(esp_reply, ok) != NULL))
        {
            return 1;
        }

        if ((fail1 != NULL) && (strstr(esp_reply, fail1) != NULL))
        {
            return 0;
        }

        if ((fail2 != NULL) && (strstr(esp_reply, fail2) != NULL))
        {
            return 0;
        }

        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return 0;
        }
    }
}


/* Send an AT command and wait for `ok`; "ERROR" / "FAIL" end the wait */
static int ESP_Command(const char *cmd, const char *ok, uint32_t timeout_ms)
{
    (void)ESP_Pump();

    ESP_ReplyReset();

    ESP_SendString(cmd);

    return ESP_WaitFor(ok, "ERROR", "FAIL", timeout_ms);
}


/* "AT" with retries: the ESP8266 may still be booting, and the first
   AT after power-up often returns ERROR */
static int ESP_Handshake(uint8_t attempts)
{
    uint8_t i;

    for (i = 0U; i < attempts; i++)
    {
        if (ESP_Command("AT\r\n", "OK", 500U) != 0)
        {
            return 1;
        }
    }

    return 0;
}


/* Send binary data over the open TCP link (single connection mode) */
static int ESP_SendTCP(const uint8_t *data, uint16_t len)
{
    char cmd[24];

    if ((esp_tcp_open == 0U) || (len == 0U) || (len > 2048U))
    {
        return 0;
    }

    (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned int)len);

    if (ESP_Command(cmd, ">", 3000U) == 0)
    {
        return 0;
    }

    ESP_ReplyReset();

    ESP_SendRaw(data, len);

    if (ESP_WaitFor("SEND OK", "SEND FAIL", "ERROR", 5000U) == 0)
    {
        return 0;
    }

    mqtt_last_tx = HAL_GetTick();

    return 1;
}


static void ESP_PrintIP(void)
{
    char        ip[20];
    uint8_t     n = 0U;
    const char *p = strstr(esp_reply, "STAIP,\"");

    if (p == NULL)
    {
        UART_Print("WARNING: could not read the IP address\r\n");
        return;
    }

    p += 7;

    while ((*p != '\0') && (*p != '"') && (n < (uint8_t)(sizeof(ip) - 1U)))
    {
        ip[n++] = *p;
        p++;
    }

    ip[n] = '\0';

    UART_Print("ESP8266 IP address: ");
    UART_Print(ip);
    UART_Print("\r\n");
}


static void ESP_PrintCWJAPError(void)
{
    const char *p = strstr(esp_reply, "+CWJAP:");

    if (p == NULL)
    {
        UART_Print("  No error code. If the ESP8266 reset during the join,\r\n");
        UART_Print("  its 3.3 V supply is too weak (Wi-Fi needs ~300 mA peaks).\r\n");
        return;
    }

    switch (p[7])
    {
        case '1': UART_Print("  +CWJAP:1 = connection timeout\r\n");         break;
        case '2': UART_Print("  +CWJAP:2 = wrong password\r\n");             break;
        case '3': UART_Print("  +CWJAP:3 = SSID \"" WIFI_SSID "\" not found "
                             "(ESP8266 is 2.4 GHz only)\r\n");              break;
        case '4': UART_Print("  +CWJAP:4 = connection failed\r\n");          break;
        default:  UART_Print("  unknown +CWJAP error code\r\n");             break;
    }
}


/* ========================================================================== */
/* +IPD PARSER  (data coming from the broker)                                 */
/*                                                                            */
/* Single connection mode (CIPMUX=0):  +IPD,<len>:<len bytes>                 */
/* The payload is binary MQTT, so exactly <len> bytes are read by count.      */
/* Returns 1 if the byte belonged to +IPD, 0 if it is normal ESP text.        */
/* ========================================================================== */

static uint8_t IPD_Feed(uint8_t c)
{
    static const char pattern[] = "+IPD,";

    switch (ipd_state)
    {
        case IPD_IDLE:

            if ((char)c == pattern[ipd_match])
            {
                ipd_match++;

                if (ipd_match == (uint8_t)(sizeof(pattern) - 1U))
                {
                    ipd_match    = 0U;
                    ipd_hdr_len  = 0U;
                    ipd_state    = IPD_HEADER;
                    esp_line_len = 0U;          /* not a text line */
                    return 1U;
                }
            }
            else
            {
                ipd_match = ((char)c == '+') ? 1U : 0U;
            }

            return 0U;


        case IPD_HEADER:

            if ((char)c == ':')
            {
                ipd_hdr[ipd_hdr_len] = '\0';

                if (IPD_ParseHeader() != 0U)
                {
                    ipd_data_len = 0U;
                    ipd_state    = IPD_PAYLOAD;
                }
                else
                {
                    ipd_state = IPD_IDLE;
                }
            }
            else if (ipd_hdr_len < (uint8_t)(sizeof(ipd_hdr) - 1U))
            {
                ipd_hdr[ipd_hdr_len++] = (char)c;
            }
            else
            {
                ipd_state = IPD_IDLE;
            }

            return 1U;


        case IPD_PAYLOAD:

            if (ipd_data_len < (uint16_t)sizeof(ipd_data))
            {
                ipd_data[ipd_data_len++] = c;
            }

            ipd_remaining--;

            if (ipd_remaining == 0U)
            {
                MQTT_HandleIncoming(ipd_data, ipd_data_len);
                ipd_state = IPD_IDLE;
            }

            return 1U;


        default:

            ipd_state = IPD_IDLE;
            break;
    }

    return 0U;
}


/* "<len>" (or "<len>,<ip>,<port>" if CIPDINFO=1) */
static uint8_t IPD_ParseHeader(void)
{
    const char *p   = ipd_hdr;
    uint32_t    len = 0U;

    if ((*p < '0') || (*p > '9'))
    {
        return 0U;
    }

    while ((*p >= '0') && (*p <= '9'))
    {
        len = (len * 10U) + (uint32_t)(*p - '0');
        p++;

        if (len > 8192U)
        {
            return 0U;
        }
    }

    if (((*p != '\0') && (*p != ',')) || (len == 0U))
    {
        return 0U;
    }

    ipd_remaining = (uint16_t)len;

    return 1U;
}


/* ========================================================================== */
/* ESP8266 INITIALIZATION  (Wi-Fi only - MQTT is done in MQTT_Connect)        */
/* ========================================================================== */

static int ESP8266_Init(void)
{
    char command[96];

    esp_ready      = 0U;
    esp_tcp_open   = 0U;
    mqtt_connected = 0U;


    UART_Print(
        "\r\nInitializing ESP8266...\r\n"
    );


    /* ---------------------------------------------------------------------- */
    /* Test ESP8266                                                           */
    /* ---------------------------------------------------------------------- */

    if (ESP_Handshake(10U) == 0)
    {
        UART_Print(
            "ERROR: ESP8266 not responding\r\n"
        );

        return 0;
    }

    UART_Print(
        "ESP8266 OK\r\n"
    );


    /* ---------------------------------------------------------------------- */
    /* Reset ESP8266 (an STM32 reset does not reset it - this clears any      */
    /* old server/connection left over from a previous run)                   */
    /* ---------------------------------------------------------------------- */

    UART_Print("Resetting ESP8266...\r\n");

    esp_dbg_mute = 1U;                  /* boot log is 74880-baud garbage */
    (void)ESP_Command("AT+RST\r\n", "OK", 2000U);
    (void)ESP_WaitFor("ready", NULL, NULL, 6000U);
    esp_dbg_mute = 0U;
    esp_line_len = 0U;
    esp_tcp_open = 0U;

    if (ESP_Handshake(10U) == 0)
    {
        UART_Print("ERROR: ESP8266 did not come back after reset\r\n");
        return 0;
    }


    /* ---------------------------------------------------------------------- */
    /* Disable echo                                                           */
    /* ---------------------------------------------------------------------- */

    (void)ESP_Command("ATE0\r\n", "OK", 1000U);


    /* ---------------------------------------------------------------------- */
    /* Station mode                                                           */
    /* ---------------------------------------------------------------------- */

    if (ESP_Command("AT+CWMODE=1\r\n", "OK", 3000U) == 0)
    {
        UART_Print(
            "ERROR: Failed to set Wi-Fi mode\r\n"
        );

        return 0;
    }


    /* ---------------------------------------------------------------------- */
    /* Connect to Wi-Fi                                                       */
    /* ---------------------------------------------------------------------- */

    (void)snprintf(
        command,
        sizeof(command),
        "AT+CWJAP=\"%s\",\"%s\"\r\n",
        WIFI_SSID,
        WIFI_PASSWORD
    );

    UART_Print(
        "Connecting to Wi-Fi \"" WIFI_SSID "\"...\r\n"
    );

    if (ESP_Command(command, "OK", 20000U) == 0)
    {
        UART_Print(
            "ERROR: Wi-Fi connection failed\r\n"
        );

        ESP_PrintCWJAPError();

        return 0;
    }

    UART_Print(
        "Wi-Fi connected\r\n"
    );


    /* ---------------------------------------------------------------------- */
    /* Normal TCP mode, single connection (client)                            */
    /* ---------------------------------------------------------------------- */

    if (ESP_Command("AT+CIPMODE=0\r\n", "OK", 3000U) == 0)
    {
        UART_Print(
            "ERROR: CIPMODE failed\r\n"
        );

        return 0;
    }

    if (ESP_Command("AT+CIPMUX=0\r\n", "OK", 3000U) == 0)
    {
        UART_Print(
            "ERROR: CIPMUX failed\r\n"
        );

        return 0;
    }


    /* ---------------------------------------------------------------------- */
    /* Show IP address                                                        */
    /* ---------------------------------------------------------------------- */

    if (ESP_Command("AT+CIFSR\r\n", "OK", 3000U) != 0)
    {
        ESP_PrintIP();
    }

    esp_ready = 1U;

    return 1;
}


/* ========================================================================== */
/* MQTT PACKET BUILDERS  (MQTT 3.1.1)                                         */
/* ========================================================================== */

/* Client ID from the STM32 96-bit unique ID */
static void MQTT_MakeClientId(void)
{
    uint32_t uid = (*(volatile uint32_t *)(UID_BASE))
                 ^ (*(volatile uint32_t *)(UID_BASE + 4U))
                 ^ (*(volatile uint32_t *)(UID_BASE + 8U));

    (void)snprintf(mqtt_client_id, sizeof(mqtt_client_id),
                   "%s%08lX", MQTT_CLIENT_PREFIX, (unsigned long)uid);
}


/* MQTT "remaining length" (1..4 bytes, 7 bits per byte) */
static uint16_t MQTT_EncodeLength(uint8_t *buf, uint32_t len)
{
    uint16_t n = 0U;

    do
    {
        uint8_t digit = (uint8_t)(len % 128U);

        len /= 128U;

        if (len > 0U)
        {
            digit |= 0x80U;
        }

        buf[n++] = digit;

    } while ((len > 0U) && (n < 4U));

    return n;
}


/* MQTT UTF-8 string: 2-byte length + bytes */
static uint16_t MQTT_WriteString(uint8_t *buf, const char *s)
{
    uint16_t len = (uint16_t)strlen(s);

    buf[0] = (uint8_t)(len >> 8);
    buf[1] = (uint8_t)(len & 0xFFU);

    memcpy(&buf[2], s, len);

    return (uint16_t)(len + 2U);
}


static uint16_t MQTT_BuildConnect(uint8_t *buf, uint16_t size)
{
    uint16_t id_len   = (uint16_t)strlen(mqtt_client_id);
    uint16_t user_len = (uint16_t)strlen(MQTT_USERNAME);
    uint16_t pass_len = (uint16_t)strlen(MQTT_PASSWORD);
    uint32_t remaining;
    uint8_t  flags = 0x02U;                 /* clean session */
    uint16_t i = 0U;

    /* variable header (10) + client ID */
    remaining = 10U + 2U + id_len;

    if (user_len > 0U)
    {
        flags |= 0x80U;
        remaining += 2U + user_len;
    }

    if ((user_len > 0U) && (pass_len > 0U))
    {
        flags |= 0x40U;
        remaining += 2U + pass_len;
    }

    if ((remaining + 5U) > size)
    {
        return 0U;
    }

    /* Fixed header: CONNECT */
    buf[i++] = 0x10U;
    i = (uint16_t)(i + MQTT_EncodeLength(&buf[i], remaining));

    /* Variable header */
    buf[i++] = 0x00U;
    buf[i++] = 0x04U;
    buf[i++] = 'M';
    buf[i++] = 'Q';
    buf[i++] = 'T';
    buf[i++] = 'T';
    buf[i++] = 0x04U;                       /* protocol level 4 = 3.1.1 */
    buf[i++] = flags;
    buf[i++] = (uint8_t)(MQTT_KEEPALIVE_S >> 8);
    buf[i++] = (uint8_t)(MQTT_KEEPALIVE_S & 0xFFU);

    /* Payload */
    i = (uint16_t)(i + MQTT_WriteString(&buf[i], mqtt_client_id));

    if ((flags & 0x80U) != 0U)
    {
        i = (uint16_t)(i + MQTT_WriteString(&buf[i], MQTT_USERNAME));
    }

    if ((flags & 0x40U) != 0U)
    {
        i = (uint16_t)(i + MQTT_WriteString(&buf[i], MQTT_PASSWORD));
    }

    return i;
}


/* PUBLISH, QoS 0, no retain */
static uint16_t MQTT_BuildPublish(uint8_t *buf, uint16_t size,
                                  const char *topic,
                                  const char *payload, uint16_t payload_len)
{
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2U + (uint32_t)topic_len + payload_len;
    uint16_t i = 0U;

    if ((remaining + 5U) > size)
    {
        return 0U;
    }

    buf[i++] = 0x30U;                       /* PUBLISH, QoS 0 */
    i = (uint16_t)(i + MQTT_EncodeLength(&buf[i], remaining));
    i = (uint16_t)(i + MQTT_WriteString(&buf[i], topic));

    memcpy(&buf[i], payload, payload_len);
    i = (uint16_t)(i + payload_len);

    return i;
}


/* Packets from the broker: CONNACK and PINGRESP */
static void MQTT_HandleIncoming(const uint8_t *data, uint16_t len)
{
    uint16_t i = 0U;

    while ((i + 1U) < len)
    {
        uint8_t  type = (uint8_t)(data[i] >> 4);
        uint32_t remaining = 0U;
        uint32_t mult = 1U;
        uint16_t j = (uint16_t)(i + 1U);

        /* decode remaining length */
        while (j < len)
        {
            remaining += (uint32_t)(data[j] & 0x7FU) * mult;
            mult *= 128U;

            if ((data[j++] & 0x80U) == 0U)
            {
                break;
            }
        }

        if ((type == 2U) && (remaining >= 2U) && ((j + 1U) < len))
        {
            /* CONNACK: [session present][return code] */
            mqtt_connack_rc       = data[j + 1U];
            mqtt_connack_received = 1U;
        }
        else
        {
            /* PINGRESP (13) and anything else: nothing to do */
        }

        i = (uint16_t)(j + remaining);
    }
}


/* ========================================================================== */
/* MQTT CONNECT                                                               */
/* ========================================================================== */

static int MQTT_Connect(void)
{
    char     command[96];
    uint16_t n;
    uint32_t start;

    mqtt_connected        = 0U;
    mqtt_connack_received = 0U;
    mqtt_connack_rc       = 0xFFU;


    /* Drop any old TCP link ("ERROR" here just means there was none) */

    esp_dbg_mute = 1U;
    (void)ESP_Command("AT+CIPCLOSE\r\n", "OK", 1000U);
    esp_dbg_mute = 0U;
    esp_tcp_open = 0U;


    /* ---------------------------------------------------------------------- */
    /* TCP connection to the broker                                           */
    /* ---------------------------------------------------------------------- */

    UART_Print("\r\nConnecting to MQTT broker "
               MQTT_BROKER ":" MQTT_STR(MQTT_PORT) " ...\r\n");

    (void)snprintf(
        command,
        sizeof(command),
        "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
        MQTT_BROKER,
        MQTT_PORT
    );

    if (ESP_Command(command, "OK", 15000U) == 0)
    {
        UART_Print("ERROR: TCP connection to broker failed\r\n");

        if (strstr(esp_reply, "DNS") != NULL)
        {
            UART_Print("  DNS failed: the network cannot resolve "
                       MQTT_BROKER "\r\n");
        }
        else
        {
            UART_Print("  If this repeats, the network may block port 1883\r\n");
            UART_Print("  (test once with a 2.4 GHz phone hotspot).\r\n");
        }

        return 0;
    }

    esp_tcp_open = 1U;

    UART_Print("TCP connected\r\n");


    /* ---------------------------------------------------------------------- */
    /* MQTT CONNECT -> wait for CONNACK                                       */
    /* ---------------------------------------------------------------------- */

    n = MQTT_BuildConnect(mqtt_packet, (uint16_t)sizeof(mqtt_packet));

    if ((n == 0U) || (ESP_SendTCP(mqtt_packet, n) == 0))
    {
        UART_Print("ERROR: sending MQTT CONNECT failed\r\n");
        return 0;
    }

    start = HAL_GetTick();

    while ((mqtt_connack_received == 0U) &&
           (esp_tcp_open != 0U) &&
           ((HAL_GetTick() - start) < 5000U))
    {
        (void)ESP_Pump();
    }

    if (mqtt_connack_received == 0U)
    {
        UART_Print("ERROR: no CONNACK from broker\r\n");
        return 0;
    }

    if (mqtt_connack_rc != 0U)
    {
        (void)snprintf(uart_buffer, sizeof(uart_buffer),
                       "ERROR: broker refused connection, CONNACK code %u\r\n",
                       (unsigned int)mqtt_connack_rc);
        UART_Print(uart_buffer);
        UART_Print("  2 = client ID rejected, 4 = bad username/password,\r\n");
        UART_Print("  5 = not authorized\r\n");
        return 0;
    }

    mqtt_connected = 1U;

    /* publish the first reading right away */
    mqtt_last_publish = HAL_GetTick() - MQTT_PUBLISH_INTERVAL_MS;

    UART_Print("\r\n==============================================\r\n");
    UART_Print("  MQTT CONNECTED\r\n");
    UART_Print("  Broker    : " MQTT_BROKER ":" MQTT_STR(MQTT_PORT) "\r\n");
    UART_Print("  Client ID : ");
    UART_Print(mqtt_client_id);
    UART_Print("\r\n");
    UART_Print("  Topic     : " MQTT_TOPIC "\r\n");
    UART_Print("  Subscribe to this topic in MQTTX to see the data\r\n");
    UART_Print("==============================================\r\n\r\n");

    return 1;
}


/* ========================================================================== */
/* MQTT PUBLISH  (same JSON format as before)                                 */
/* ========================================================================== */

static void MQTT_Publish(void)
{
    char     json[128];
    int      json_len;
    uint16_t n;
    int      ok;
    int32_t  temp_centi;
    uint32_t temp_abs;
    uint32_t pressure_pa;
    uint32_t humidity_centi;


    if (bme_data_valid == 0U)
    {
        return;
    }


    /* Temperature */

    temp_centi =
        (int32_t)(
            comp_data.temperature * 100.0
        );

    temp_abs =
        (temp_centi < 0) ?
        (uint32_t)(-temp_centi) :
        (uint32_t)temp_centi;


    /* Pressure (Pa -> hPa with 2 decimals) */

    pressure_pa =
        (uint32_t)(
            comp_data.pressure + 0.5
        );


    /* Humidity */

    humidity_centi =
        (uint32_t)(
            comp_data.humidity * 100.0 + 0.5
        );


    /* JSON */

    json_len = snprintf(
        json,
        sizeof(json),

        "{"
        "\"temperature\":%s%lu.%02lu,"
        "\"pressure\":%lu.%02lu,"
        "\"humidity\":%lu.%02lu"
        "}",

        (temp_centi < 0) ? "-" : "",

        (unsigned long)(temp_abs / 100U),
        (unsigned long)(temp_abs % 100U),

        (unsigned long)(pressure_pa / 100U),
        (unsigned long)(pressure_pa % 100U),

        (unsigned long)(humidity_centi / 100U),
        (unsigned long)(humidity_centi % 100U)
    );

    if ((json_len <= 0) || (json_len >= (int)sizeof(json)))
    {
        UART_Print("ERROR: JSON too large\r\n");
        return;
    }


    /* PUBLISH packet */

    n = MQTT_BuildPublish(mqtt_packet, (uint16_t)sizeof(mqtt_packet),
                          MQTT_TOPIC, json, (uint16_t)json_len);

    if (n == 0U)
    {
        UART_Print("ERROR: MQTT packet too large\r\n");
        return;
    }

    esp_dbg_mute = 1U;              /* keep PuTTY clean on routine sends */
    ok = ESP_SendTCP(mqtt_packet, n);
    esp_dbg_mute = 0U;

    if (ok != 0)
    {
        mqtt_publish_count++;

        (void)snprintf(uart_buffer, sizeof(uart_buffer),
                       "MQTT publish #%lu -> ",
                       (unsigned long)mqtt_publish_count);
        UART_Print(uart_buffer);
        UART_Print(MQTT_TOPIC " : ");
        UART_Print(json);
        UART_Print("\r\n");
    }
    else
    {
        UART_Print("ERROR: MQTT publish failed - reconnecting\r\n");
        mqtt_connected = 0U;
    }
}


/* PINGREQ - only needed if nothing was sent for half the keep-alive */
static void MQTT_Ping(void)
{
    static const uint8_t pingreq[2] = { 0xC0U, 0x00U };
    int ok;

    esp_dbg_mute = 1U;
    ok = ESP_SendTCP(pingreq, 2U);
    esp_dbg_mute = 0U;

    if (ok == 0)
    {
        UART_Print("ERROR: MQTT ping failed - reconnecting\r\n");
        mqtt_connected = 0U;
    }
}


/* ========================================================================== */
/* MQTT PROCESS  (call every loop)                                            */
/* ========================================================================== */

static void MQTT_Process(void)
{
    uint32_t now;

    (void)ESP_Pump();

    now = HAL_GetTick();


    /* Wi-Fi not up: full ESP8266 init again */

    if (esp_ready == 0U)
    {
        if ((now - mqtt_last_retry) >= MQTT_RETRY_INTERVAL_MS)
        {
            if (ESP8266_Init() != 0)
            {
                mqtt_connect_fails = 0U;
                (void)MQTT_Connect();
            }

            mqtt_last_retry = HAL_GetTick();
        }

        return;
    }


    /* Broker not connected: reconnect */

    if (mqtt_connected == 0U)
    {
        if ((now - mqtt_last_retry) >= MQTT_RETRY_INTERVAL_MS)
        {
            if (MQTT_Connect() != 0)
            {
                mqtt_connect_fails = 0U;
            }
            else
            {
                mqtt_connect_fails++;

                if (mqtt_connect_fails >= MQTT_MAX_CONNECT_FAILS)
                {
                    UART_Print("Too many failures - re-joining Wi-Fi\r\n");
                    mqtt_connect_fails = 0U;
                    esp_ready = 0U;
                }
            }

            mqtt_last_retry = HAL_GetTick();
        }

        return;
    }


    /* Connected: publish, or keep the connection alive */

    if ((now - mqtt_last_publish) >= MQTT_PUBLISH_INTERVAL_MS)
    {
        mqtt_last_publish = now;
        MQTT_Publish();
    }
    else if ((now - mqtt_last_tx) >= ((MQTT_KEEPALIVE_S * 1000U) / 2U))
    {
        MQTT_Ping();
    }
    else
    {
        /* nothing to do */
    }
}


/* ========================================================================== */
/* ERROR HANDLER                                                              */
/* ========================================================================== */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}


#ifdef USE_FULL_ASSERT

void assert_failed(
    uint8_t *file,
    uint32_t line)
{
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* USER CODE END 6 */
}

#endif