
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* =====================================================
                       USER SETTINGS
   ===================================================== */

#define WIFI_SSID       "SNUC"
#define WIFI_PASSWORD   "snu12345"

#define MQTT_BROKER     "broker.emqx.io"  
#define MQTT_PORT       1883

#define MQTT_CLIENT_ID  "SNAKE_PLAYER_A_2026"
#define MQTT_TOPIC      "arena/snake/playerAx"

#define JOY_LOW         1200
#define JOY_HIGH        2800

#define INVERT_Y        0

#define MQTT_KEEPALIVE  60

/* =====================================================
                    PERIPHERAL HANDLES
   ===================================================== */

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;

/* =====================================================
                     GLOBAL VARIABLES
   ===================================================== */

char currentDirection[10] = "";
char previousDirection[10] = "";

static uint8_t mqtt_rx_window[4] = {0};
static uint8_t mqtt_rx_count = 0;

static bool mqtt_connack_received = false;
static bool mqtt_pingresp_received = false;

static uint32_t last_mqtt_activity = 0;

/* =====================================================
                  FUNCTION DECLARATIONS
   ===================================================== */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);

void Error_Handler(void);

uint16_t Read_ADC(uint32_t channel);
void Read_Joystick(void);

bool ESP_SendATWait(char *command,
                    char *expected,
                    uint32_t timeout);

bool ESP_SendRaw(uint8_t *data, uint16_t length);

void ESP_ProcessReceivedByte(uint8_t ch);

bool ESP_WaitMQTT(bool *flag, uint32_t timeout);

void ESP_Connect(void);

bool MQTT_Connect(void);
bool MQTT_Publish(char *direction);
bool MQTT_Ping(void);

/* =====================================================
                           MAIN
   ===================================================== */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    HAL_Delay(1000);

    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    ESP_Connect();

    last_mqtt_activity = HAL_GetTick();

    while (1)
    {
        Read_Joystick();

        if (strcmp(currentDirection, "") != 0 &&
            strcmp(currentDirection, previousDirection) != 0)
        {
            if (MQTT_Publish(currentDirection))
            {
                strcpy(previousDirection, currentDirection);
                last_mqtt_activity = HAL_GetTick();
            }
        }

        /* Send MQTT keepalive every 30 seconds */
        if ((HAL_GetTick() - last_mqtt_activity) >= 30000)
        {
            if (MQTT_Ping())
            {
                last_mqtt_activity = HAL_GetTick();
            }
            else
            {
                Error_Handler();
            }
        }

        HAL_Delay(100);
    }
}

/* =====================================================
                  MQTT RESPONSE BYTE PARSER
   ===================================================== */

void ESP_ProcessReceivedByte(uint8_t ch)
{
    if (mqtt_rx_count < 4)
    {
        mqtt_rx_window[mqtt_rx_count++] = ch;
    }
    else
    {
        mqtt_rx_window[0] = mqtt_rx_window[1];
        mqtt_rx_window[1] = mqtt_rx_window[2];
        mqtt_rx_window[2] = mqtt_rx_window[3];
        mqtt_rx_window[3] = ch;
    }

    if (mqtt_rx_count == 4)
    {
        /* MQTT CONNACK: 20 02 00 00 */
        if (mqtt_rx_window[0] == 0x20 &&
            mqtt_rx_window[1] == 0x02 &&
            mqtt_rx_window[2] == 0x00 &&
            mqtt_rx_window[3] == 0x00)
        {
            mqtt_connack_received = true;
        }
    }

    if (mqtt_rx_count >= 2)
    {
        /* MQTT PINGRESP: D0 00 */
        if (mqtt_rx_window[mqtt_rx_count - 2] == 0xD0 &&
            mqtt_rx_window[mqtt_rx_count - 1] == 0x00)
        {
            mqtt_pingresp_received = true;
        }
    }
}

/* =====================================================
                   ESP8266 AT COMMAND HANDLER
   ===================================================== */

bool ESP_SendATWait(char *command,
                    char *expected,
                    uint32_t timeout)
{
    char response[180] = {0};
    uint8_t ch;
    uint16_t index = 0;

    if (strlen(command) > 0)
    {
        if (HAL_UART_Transmit(&huart1,
                              (uint8_t *)command,
                              strlen(command),
                              3000) != HAL_OK)
        {
            return false;
        }
    }

    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 20) == HAL_OK)
        {
            ESP_ProcessReceivedByte(ch);

            if (index < sizeof(response) - 1)
            {
                response[index++] = ch;
                response[index] = '\0';
            }
            else
            {
                memmove(response,
                        response + 1,
                        sizeof(response) - 2);

                response[sizeof(response) - 2] = ch;
                response[sizeof(response) - 1] = '\0';
            }

            if (strstr(response, expected) != NULL)
            {
                return true;
            }

            if (strstr(response, "ERROR") != NULL ||
                strstr(response, "FAIL") != NULL ||
                strstr(response, "CLOSED") != NULL)
            {
                return false;
            }
        }
    }

    return false;
}

/* =====================================================
                    SEND RAW TCP DATA
   ===================================================== */

bool ESP_SendRaw(uint8_t *data, uint16_t length)
{
    char command[40];

    snprintf(command,
             sizeof(command),
             "AT+CIPSEND=%u\r\n",
             length);

    /* Request permission to send */
    if (!ESP_SendATWait(command, ">", 5000))
    {
        return false;
    }

    /* Send binary MQTT packet */
    if (HAL_UART_Transmit(&huart1,
                          data,
                          length,
                          5000) != HAL_OK)
    {
        return false;
    }

    /* ESP-AT confirms transmission to TCP stack */
    if (!ESP_SendATWait("", "SEND OK", 5000))
    {
        return false;
    }

    return true;
}

/* =====================================================
                   WAIT FOR MQTT RESPONSE
   ===================================================== */

bool ESP_WaitMQTT(bool *flag, uint32_t timeout)
{
    uint8_t ch;

    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout)
    {
        if (*flag)
        {
            return true;
        }

        if (HAL_UART_Receive(&huart1, &ch, 1, 20) == HAL_OK)
        {
            ESP_ProcessReceivedByte(ch);

            if (*flag)
            {
                return true;
            }

            if (ch == 0)
            {
                continue;
            }
        }
    }

    return *flag;
}

/* =====================================================
                       MQTT CONNECT
   ===================================================== */

bool MQTT_Connect(void)
{
    uint8_t packet[128];
    uint16_t i = 0;

    char clientID[] = MQTT_CLIENT_ID;

    uint8_t clientLength = strlen(clientID);

    /* Fixed header */
    packet[i++] = 0x10;

    /* Remaining length:
       Variable header = 10 bytes
       Client ID length field = 2 bytes
       Client ID = clientLength
    */
    packet[i++] = 12 + clientLength;

    /* Protocol name */
    packet[i++] = 0x00;
    packet[i++] = 0x04;

    packet[i++] = 'M';
    packet[i++] = 'Q';
    packet[i++] = 'T';
    packet[i++] = 'T';

    /* MQTT 3.1.1 */
    packet[i++] = 0x04;

    /* Clean session */
    packet[i++] = 0x02;

    /* Keepalive */
    packet[i++] = 0x00;
    packet[i++] = MQTT_KEEPALIVE;

    /* Client ID */
    packet[i++] = 0x00;
    packet[i++] = clientLength;

    memcpy(&packet[i], clientID, clientLength);
    i += clientLength;

    mqtt_connack_received = false;
    mqtt_rx_count = 0;

    if (!ESP_SendRaw(packet, i))
    {
        return false;
    }

    /* Wait for CONNACK inside ESP-AT +IPD data */
    if (!ESP_WaitMQTT(&mqtt_connack_received, 5000))
    {
        return false;
    }

    return true;
}

/* =====================================================
                       MQTT PUBLISH
   ===================================================== */

bool MQTT_Publish(char *direction)
{
    uint8_t packet[128];

    char payload[50];
    char topic[] = MQTT_TOPIC;

    uint16_t i = 0;

    snprintf(payload,
             sizeof(payload),
             "{\"direction\":\"%s\"}",
             direction);

    uint16_t topicLength = strlen(topic);
    uint16_t payloadLength = strlen(payload);

    uint16_t remainingLength =
        2 + topicLength + payloadLength;

    /* Fixed header: PUBLISH, QoS 0 */
    packet[i++] = 0x30;

    packet[i++] = remainingLength;

    /* Topic length */
    packet[i++] = (topicLength >> 8) & 0xFF;
    packet[i++] = topicLength & 0xFF;

    /* Topic */
    memcpy(&packet[i], topic, topicLength);
    i += topicLength;

    /* JSON payload */
    memcpy(&packet[i], payload, payloadLength);
    i += payloadLength;

    return ESP_SendRaw(packet, i);
}

/* =====================================================
                       MQTT PING
   ===================================================== */

bool MQTT_Ping(void)
{
    uint8_t packet[] = {0xC0, 0x00};

    mqtt_pingresp_received = false;

    if (!ESP_SendRaw(packet, sizeof(packet)))
    {
        return false;
    }

    return ESP_WaitMQTT(&mqtt_pingresp_received, 5000);
}

/* =====================================================
                 WIFI + TCP + MQTT CONNECTION
   ===================================================== */

void ESP_Connect(void)
{
    char command[250];

    if (!ESP_SendATWait("AT\r\n", "OK", 3000))
    {
        Error_Handler();
    }

    if (!ESP_SendATWait("ATE0\r\n", "OK", 3000))
    {
        Error_Handler();
    }

    if (!ESP_SendATWait("AT+CWMODE=1\r\n", "OK", 3000))
    {
        Error_Handler();
    }

    /* Wi-Fi connection */
    snprintf(command,
             sizeof(command),
             "AT+CWJAP=\"%s\",\"%s\"\r\n",
             WIFI_SSID,
             WIFI_PASSWORD);

    if (!ESP_SendATWait(command, "OK", 30000))
    {
        Error_Handler();
    }

    HAL_Delay(1000);

    /* Single connection mode */
    if (!ESP_SendATWait("AT+CIPMUX=0\r\n", "OK", 3000))
    {
        Error_Handler();
    }

    /* Normal TCP mode */
    if (!ESP_SendATWait("AT+CIPMODE=0\r\n", "OK", 3000))
    {
        Error_Handler();
    }

    /* Connect to broker */
    snprintf(command,
             sizeof(command),
             "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
             MQTT_BROKER,
             MQTT_PORT);

    if (!ESP_SendATWait(command, "OK", 15000))
    {
        Error_Handler();
    }

    HAL_Delay(500);

    /* MQTT CONNECT */
    if (!MQTT_Connect())
    {
        Error_Handler();
    }
}

/* =====================================================
                      ADC READING
   ===================================================== */

uint16_t Read_ADC(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        Error_Handler();
    }

    uint16_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}

/* =====================================================
                    JOYSTICK READING
   ===================================================== */

void Read_Joystick(void)
{
    uint16_t xValue;
    uint16_t yValue;

    xValue = Read_ADC(ADC_CHANNEL_0);
    yValue = Read_ADC(ADC_CHANNEL_1);

#if INVERT_Y
    yValue = 4095 - yValue;
#endif

    if (xValue < JOY_LOW)
    {
        strcpy(currentDirection, "LEFT");
    }
    else if (xValue > JOY_HIGH)
    {
        strcpy(currentDirection, "RIGHT");
    }
    else if (yValue < JOY_LOW)
    {
        strcpy(currentDirection, "UP");
    }
    else if (yValue > JOY_HIGH)
    {
        strcpy(currentDirection, "DOWN");
    }
}

/* =====================================================
                     ADC1 INITIALIZATION
   ===================================================== */

static void MX_ADC1_Init(void)
{
    hadc1.Instance = ADC1;

    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =====================================================
                    USART1 INITIALIZATION
   ===================================================== */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =====================================================
                     GPIO INITIALIZATION
   ===================================================== */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PB0 retained as input with pull-up */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* =====================================================
                   SYSTEM CLOCK 72 MHz
   ===================================================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
}

/* =====================================================
                      ERROR HANDLER
   ===================================================== */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
