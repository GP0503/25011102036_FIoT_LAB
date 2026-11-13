/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bme280.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PC_UART    (&huart2)     /* <-- serial terminal (115200)      */
#define WIFI_UART  (&huart1)     /* <-- ESP8266 / Wemos (115200)      */
#define WIFI_SSID  "SNUC"        /* <-- 2.4 GHz network / hotspot     */
#define WIFI_PASS  "snu12345"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
struct bme280_dev      dev;
struct bme280_data     comp_data;
struct bme280_settings settings;
int8_t rslt = BME280_OK;

float temperature, pressure, humidity;

char line[160];            /* UART print buffer        */
char rx[1024];             /* ESP8266 reply buffer     */
char json[200];
char http_response[300];
char cmd[64];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void serve_http(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---------- PC terminal ---------- */
void pc_print(const char *s)
{
  HAL_UART_Transmit(PC_UART, (uint8_t *)s, strlen(s), 1000);
}

/* ---------- ESP8266 helpers ---------- */
void wifi_send_raw(const char *s)
{
  HAL_UART_Transmit(WIFI_UART, (uint8_t *)s, strlen(s), 1000);
}

/* Read from ESP until `token` appears (returns 1) or timeout (returns 0) */
int wifi_wait(const char *token, uint32_t timeout_ms)
{
  uint16_t i = 0;
  uint8_t c;
  uint32_t start = HAL_GetTick();
  rx[0] = '\0';
  while ((HAL_GetTick() - start) < timeout_ms && i < sizeof(rx) - 1)
  {
    if (HAL_UART_Receive(WIFI_UART, &c, 1, 5) == HAL_OK)
    {
      rx[i++] = c;
      rx[i] = '\0';
      if (strstr(rx, token)) return 1;
      if (strstr(rx, "ERROR\r\n") || strstr(rx, "FAIL\r\n")) return 0;
    }
  }
  return 0;
}

/* Send an AT command, wait for OK, echo everything to the PC */
int at_cmd(const char *c, uint32_t timeout_ms)
{
  int ok;
  pc_print("\r\n>> "); pc_print(c); pc_print("\r\n");
  wifi_send_raw(c);
  wifi_send_raw("\r\n");
  ok = wifi_wait("OK\r\n", timeout_ms);
  pc_print("<< "); pc_print(rx); pc_print("\r\n");
  return ok;
}

/* ---------- Bosch driver glue (porting layer) ---------- */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  uint8_t dev_addr = *(uint8_t *)intf_ptr;
  if (HAL_I2C_Mem_Read(&hi2c1, (dev_addr << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
                       reg_data, len, 100) == HAL_OK)
    return BME280_OK;
  return BME280_E_COMM_FAIL;
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  uint8_t dev_addr = *(uint8_t *)intf_ptr;
  if (HAL_I2C_Mem_Write(&hi2c1, (dev_addr << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
                        (uint8_t *)reg_data, len, 100) == HAL_OK)
    return BME280_OK;
  return BME280_E_COMM_FAIL;
}

void user_delay_us(uint32_t period, void *intf_ptr)
{
  HAL_Delay((period / 1000) + 1);   /* us -> ms, rounded up */
}

/* ---------- Serve one browser request ---------- */
void serve_http(void)
{
  int link_id = 0, content_length, response_length;
  uint8_t c = '0';

  /* "+IPD," was just matched; the next byte is the link id: "+IPD,0,..." */
  if (HAL_UART_Receive(WIFI_UART, &c, 1, 100) == HAL_OK)
  {
    link_id = c - '0';
  }

  wifi_wait("\r\n\r\n", 500);             /* swallow rest of the request */

  snprintf(json, sizeof(json),
           "{\"status\":\"success\","
           "\"temperature\":%.2f,"
           "\"humidity\":%.2f,"
           "\"pressure\":%.2f}",
           temperature, humidity, pressure);
  content_length = strlen(json);

  snprintf(http_response, sizeof(http_response),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %d\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           content_length, json);
  response_length = strlen(http_response);

  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d\r\n", link_id, response_length);
  wifi_send_raw(cmd);
  if (wifi_wait(">", 2000))
  {
    wifi_send_raw(http_response);
    wifi_wait("SEND OK", 3000);
    pc_print("[HTTP] sent: "); pc_print(json); pc_print("\r\n");
  }
  else
  {
    pc_print("[HTTP] CIPSEND failed\r\n");
  }

  snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d\r\n", link_id);
  wifi_send_raw(cmd);
  wifi_wait("OK\r\n", 1000);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  
  /* USER CODE BEGIN 2 */
  static uint8_t dev_addr = BME280_I2C_ADDR_PRIM;   /* 0x76 (0x77 if SDO high) */
  uint8_t chip_id = 0;

  pc_print("\r\n==== CAT12 BME280 ====\r\n");

  /* Step 1: read chip ID directly */
  if (HAL_I2C_Mem_Read(&hi2c1, dev_addr << 1, 0xD0, I2C_MEMADD_SIZE_8BIT,
                       &chip_id, 1, 100) == HAL_OK)
  {
    snprintf(line, sizeof(line), "I2C OK, chip ID = 0x%02X %s\r\n", chip_id,
             chip_id == 0x60 ? "(BME280)" : chip_id == 0x58 ? "(BMP280 - no humidity!)" : "(unknown)");
  }
  else
  {
    snprintf(line, sizeof(line), "I2C FAIL at 0x%02X - check wiring / try 0x77\r\n", dev_addr);
  }
  pc_print(line);

  /* Step 2: hand the driver our glue functions */
  dev.intf_ptr = &dev_addr;
  dev.intf     = BME280_I2C_INTF;
  dev.read     = user_i2c_read;
  dev.write    = user_i2c_write;
  dev.delay_us = user_delay_us;

  rslt = bme280_init(&dev);                  /* checks ID + loads trim data */
  snprintf(line, sizeof(line), "bme280_init: %d %s\r\n", rslt, rslt == BME280_OK ? "OK" : "FAIL");
  pc_print(line);

  /* Step 3: settings */
  rslt = bme280_get_sensor_settings(&settings, &dev);
  settings.osr_h        = BME280_OVERSAMPLING_1X;
  settings.osr_p        = BME280_OVERSAMPLING_1X;
  settings.osr_t        = BME280_OVERSAMPLING_1X;
  settings.filter       = BME280_FILTER_COEFF_OFF;
  settings.standby_time = BME280_STANDBY_TIME_1000_MS;
  rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
  snprintf(line, sizeof(line), "settings: %d\r\n", rslt);
  pc_print(line);

  rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
  snprintf(line, sizeof(line), "normal mode: %d\r\n", rslt);
  pc_print(line);

  /* Step 4: Wi-Fi + HTTP server on port 80 */
  HAL_Delay(1000);
  at_cmd("AT", 1000);
  at_cmd("ATE0", 1000);                      /* echo off */
  at_cmd("AT+CWMODE=1", 1000);
  at_cmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASS "\"", 20000);
  at_cmd("AT+CIFSR", 2000);                  /* <-- IP to type in the browser */
  at_cmd("AT+CIPMUX=1", 1000);
  at_cmd("AT+CIPSERVER=1,80", 2000);
  pc_print("\r\nServer ready: open http://<STAIP above>/ in a browser\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    if (rslt == BME280_OK)
    {
      temperature = comp_data.temperature;            /* deg C */
      pressure    = comp_data.pressure / 100.0f;      /* Pa -> hPa */
      humidity    = comp_data.humidity;               /* %RH */

      snprintf(line, sizeof(line),
               "Temperature : %.2f C\r\nPressure    : %.2f hPa\r\nHumidity    : %.2f %%RH\r\n\r\n",
               temperature, pressure, humidity);
      pc_print(line);
    }
    else
    {
      snprintf(line, sizeof(line), "read error %d\r\n", rslt);
      pc_print(line);
    }

    /* Wait ~1 s, but answer the browser if it connects meanwhile */
    if (wifi_wait("+IPD,", 1000))
      serve_http();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}