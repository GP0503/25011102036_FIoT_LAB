/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  *                   FPIoT Lab Evaluation CAT-8 - Task 8B (Event-Based Control)
  *                   Every valid switch press on PB15 toggles the on-board LED
  *                   on PC13 exactly once. Switch bounce is removed in software
  *                   by requiring the raw input to stay unchanged for
  *                   DEBOUNCE_MS before it is accepted as a new stable state.
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

/* On-board LED of the Blue Pill : PC13, active LOW (pin low -> LED lit) */
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_PIN_13
#define LED_ON_LEVEL          GPIO_PIN_RESET
#define LED_OFF_LEVEL         GPIO_PIN_SET

/* Push-button switch : PB15, configured as input with no internal pull */
#define SWITCH_PORT           GPIOB
#define SWITCH_PIN            GPIO_PIN_15

/* Level read on PB15 when the switch is pressed.
   Change to GPIO_PIN_RESET if your switch pulls PB15 to GND instead of 3.3 V. */
#define SWITCH_PRESSED_LEVEL  GPIO_PIN_SET

/* Software debounce time : a new state is accepted only if the raw input
   has remained stable for this long (typical bounce is 1 - 20 ms) */
#define DEBOUNCE_MS           30u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Debounce state machine */
static uint8_t  switch_raw_last = 0u;    /* last raw level read from PB15       */
static uint8_t  switch_stable   = 0u;    /* debounced (accepted) switch state   */
static uint32_t switch_change_tick = 0u; /* tick of the last raw level change   */
static uint8_t  led_state = 0u;          /* remembered LED state (0 = off)      */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void Led_Write(uint8_t on);
static void Led_Toggle(void);
static uint8_t Switch_IsPressed(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Drives the on-board LED to the requested logical state.
  * @param  on 0 = LED off, any other value = LED on
  * @retval None
  */
static void Led_Write(uint8_t on)
{
  led_state = (on != 0u) ? 1u : 0u;
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, (led_state != 0u) ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

/**
  * @brief  Inverts the on-board LED state.
  * @retval None
  */
static void Led_Toggle(void)
{
  led_state = (led_state != 0u) ? 0u : 1u;
  HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

/**
  * @brief  Reads the switch and converts it to a logical "pressed" flag.
  * @retval 1 when the switch is pressed, 0 otherwise
  */
static uint8_t Switch_IsPressed(void)
{
  return (HAL_GPIO_ReadPin(SWITCH_PORT, SWITCH_PIN) == SWITCH_PRESSED_LEVEL) ? 1u : 0u;
}

/**
  * @brief  Non-blocking debounce of the switch.
  *         The raw input is sampled and a new state is accepted only when it
  *         has stayed stable for DEBOUNCE_MS, so contact bounce produces a
  *         single edge instead of a burst of edges.
  * @retval 1 on the edge where a valid press is confirmed, 0 otherwise
  */
static uint8_t Switch_ScanPressEvent(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  raw = Switch_IsPressed();

  /* Remember when the raw input last changed */
  if (raw != switch_raw_last)
  {
    switch_raw_last    = raw;
    switch_change_tick = now;
  }

  /* Accept the new level only once it has been stable long enough */
  if (((uint32_t)(now - switch_change_tick) >= DEBOUNCE_MS) && (switch_raw_last != switch_stable))
  {
    switch_stable = switch_raw_last;

    if (switch_stable != 0u) /* 0 -> 1 : one press detected */
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

  /* Initialise the debounce state machine from the current input level,
     so the application does not report a false press at start-up */
  switch_stable = Switch_IsPressed();
  switch_raw_last = switch_stable;
  switch_change_tick = HAL_GetTick();

  /* LED starts from a known state */
  Led_Write(0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Task 8B : every debounced press toggles the LED exactly once.
       Holding the switch does not produce repeated toggles because only the
       release-to-press transition generates an event. */
    if (Switch_ScanPressEvent() != 0u)
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
