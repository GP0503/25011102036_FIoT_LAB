/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  *                   FPIoT Lab Evaluation CAT-11 - Task 2
  *                   On-board Switch Control with State Remembrance
  *
  *                   Each valid press of the user button B1 (PC13) toggles the
  *                   on-board LED LD2 (PA5). The LED keeps (remembers) its
  *                   state after the button is released, because the press is
  *                   handled as a single edge event instead of a level.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* On-board user LED LD2 of the NUCLEO-F401RE : PA5, active HIGH */
#define LED_PORT              GPIOA
#define LED_PIN               GPIO_PIN_5
#define LED_ON_LEVEL          GPIO_PIN_SET
#define LED_OFF_LEVEL         GPIO_PIN_RESET

/* On-board user button B1 of the NUCLEO-F401RE : PC13, active LOW
   (the board provides the pull-up, so the pin reads LOW while pressed) */
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_13
#define BUTTON_PRESSED_LEVEL  GPIO_PIN_RESET

/* Software debounce window for the mechanical button */
#define DEBOUNCE_MS           30u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Debounce state machine */
static uint8_t  button_raw_last     = 0u;  /* last raw level read from PC13     */
static uint8_t  button_stable       = 0u;  /* debounced (accepted) state        */
static uint32_t button_change_tick  = 0u;  /* tick of the last raw level change */

/* Remembered output state : survives the button release */
static uint8_t  led_state = 0u;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void Led_Write(uint8_t on);
static void Led_Toggle(void);
static uint8_t Button_IsPressed(void);
static uint8_t Button_ScanPressEvent(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Drives the on-board user LED to the requested logical state.
  * @param  on 0 = LED off, any other value = LED on
  * @retval None
  */
static void Led_Write(uint8_t on)
{
  led_state = (on != 0u) ? 1u : 0u;
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, (led_state != 0u) ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

/**
  * @brief  Inverts the on-board user LED state.
  * @retval None
  */
static void Led_Toggle(void)
{
  led_state = (led_state != 0u) ? 0u : 1u;
  HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

/**
  * @brief  Reads the user button and converts it to a logical "pressed" flag.
  * @retval 1 when the button is pressed, 0 otherwise
  */
static uint8_t Button_IsPressed(void)
{
  return (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == BUTTON_PRESSED_LEVEL) ? 1u : 0u;
}

/**
  * @brief  Non-blocking debounce plus edge detection.
  *         A new level is accepted only after it stayed stable for DEBOUNCE_MS,
  *         and a press event is reported once per press (not once per loop
  *         iteration and not repeatedly while the button is held).
  * @retval 1 on the confirmed press edge, 0 otherwise
  */
static uint8_t Button_ScanPressEvent(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  raw = Button_IsPressed();

  /* Remember when the raw level last changed */
  if (raw != button_raw_last)
  {
    button_raw_last    = raw;
    button_change_tick = now;
  }

  /* Accept the new level only once it has been stable long enough */
  if (((uint32_t)(now - button_change_tick) >= DEBOUNCE_MS) && (button_raw_last != button_stable))
  {
    button_stable = button_raw_last;

    if (button_stable != 0u) /* released -> pressed : one valid press */
    {
      return 1u;
    }
  }

  return 0u;
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
  /* USER CODE BEGIN 2 */

  /* Seed the debounce state machine with the current button level so that a
     button already held at reset does not generate a false press */
  button_stable      = Button_IsPressed();
  button_raw_last    = button_stable;
  button_change_tick = HAL_GetTick();

  /* LED starts from a known state and remembers it */
  Led_Write(0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Task 2 : state remembrance.
       The LED is toggled only when a valid press edge is detected and it keeps
       that state after the button is released, because the output is driven
       from the remembered variable led_state and not from the button level. */
    if (Button_ScanPressEvent() != 0u)
    {
      Led_Toggle();
    }
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
