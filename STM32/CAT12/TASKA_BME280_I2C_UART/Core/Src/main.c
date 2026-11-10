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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* BME280 device structure */
static struct bme280_dev bme280_dev;

/* Compensated sensor readings */
static struct bme280_data comp_data;

/* Sensor configuration */
static struct bme280_settings settings;

/* I2C address: SDO connected to GND */
static uint8_t bme280_i2c_address =
    BME280_I2C_ADDR_PRIM;

/* API return status */
static int8_t rslt;

/* UART output buffer */
static char uart_buffer[128];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
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

static void UART_Print(const char *message);

static void BME280_Init(void);

static void BME280_PrintData(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	UART_Print("\r\nSTM32 BME280 PROJECT\r\n");
UART_Print("Starting sensor initialization...\r\n");

/* Check whether BME280 responds on I2C */
if (HAL_I2C_IsDeviceReady(
        &hi2c1,
        (uint16_t)(bme280_i2c_address << 1),
        3,
        100U) != HAL_OK)
{
    UART_Print("ERROR: BME280 I2C device not responding\r\n");
    Error_Handler();
}

/* Initialize and configure BME280 */
BME280_Init();

/* Allow the first measurement cycle to complete */
HAL_Delay(1100U);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		/* Read all compensated sensor data */

rslt = bme280_get_sensor_data(
    BME280_ALL,
    &comp_data,
    &bme280_dev
);

if (rslt == BME280_OK)
{
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

    if ((len > 0) && ((size_t)len < sizeof(uart_buffer)))
    {
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)uart_buffer,
            (uint16_t)len,
            1000U
        );
    }
}

/* Wait for the next normal-mode measurement */
HAL_Delay(1100U);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
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
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
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

    dev_addr = *(uint8_t *)intf_ptr;

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
//Implement the I²C write function
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

    dev_addr = *(uint8_t *)intf_ptr;

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
//Implement the delay function
static void user_delay_us(
    uint32_t period,
    void *intf_ptr)
{
    (void)intf_ptr;

    if (period > 0U)
    {
        HAL_Delay((period + 999U) / 1000U);
    }
}
// Implement UART output
static void UART_Print(const char *message)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)message,
        (uint16_t)strlen(message),
        1000U
    );
}
//Initialize the BME280
static void BME280_Init(void)
{
    memset(&bme280_dev, 0, sizeof(bme280_dev));

    /* Configure Bosch driver interface */
    bme280_dev.intf_ptr = &bme280_i2c_address;

    bme280_dev.intf = BME280_I2C_INTF;

    bme280_dev.read = user_i2c_read;

    bme280_dev.write = user_i2c_write;

    bme280_dev.delay_us = user_delay_us;

    /* Initialize sensor and read calibration data */
    rslt = bme280_init(&bme280_dev);

    if (rslt != BME280_OK)
    {
        UART_Print("ERROR: BME280 initialization failed\r\n");
        Error_Handler();
    }

    if (bme280_dev.chip_id != BME280_CHIP_ID)
    {
        UART_Print("ERROR: Invalid sensor ID\r\n");
        Error_Handler();
    }

    UART_Print("BME280 initialization successful\r\n");
    UART_Print("Sensor ID verified: 0x60\r\n");

    /* Configure oversampling */
    settings.osr_t = BME280_OVERSAMPLING_1X;

    settings.osr_p = BME280_OVERSAMPLING_1X;

    settings.osr_h = BME280_OVERSAMPLING_1X;

    /* Disable internal filter */
    settings.filter = BME280_FILTER_COEFF_OFF;

    /* Configure standby interval */
    settings.standby_time = BME280_STANDBY_TIME_1000_MS;

    /* Apply sensor configuration */
    rslt = bme280_set_sensor_settings(
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
        UART_Print("ERROR: Sensor configuration failed\r\n");
        Error_Handler();
    }

    /* Enable continuous measurements */
    rslt = bme280_set_sensor_mode(
        BME280_POWERMODE_NORMAL,
        &bme280_dev
    );

    if (rslt != BME280_OK)
    {
        UART_Print("ERROR: Sensor mode configuration failed\r\n");
        Error_Handler();
    }

    UART_Print("BME280 configured successfully\r\n");
}
//Implement the measurement display function
static void BME280_PrintData(void)
{
    int32_t temp_centi;

    uint32_t temp_abs;
    uint32_t pressure_pa;
    uint32_t humidity_centi;

    int len;

    /* Temperature resolution: 0.01 degree C */
    temp_centi = (int32_t)(comp_data.temperature * 100.0);

    temp_abs = (temp_centi < 0) ?
               (uint32_t)(-temp_centi) :
               (uint32_t)temp_centi;

    /* Pressure from Bosch driver is in Pa */
    pressure_pa = (uint32_t)(comp_data.pressure + 0.5);

    /* Humidity in percentage relative humidity */
    humidity_centi =
        (uint32_t)(comp_data.humidity * 100.0 + 0.5);

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

    if ((len > 0) && ((size_t)len < sizeof(uart_buffer)))
    {
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)uart_buffer,
            (uint16_t)len,
            1000U
        );
    }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */