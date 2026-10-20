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
#include "dma.h"
#include "tim.h"
#include "gpio.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef uint16_t u16;
typedef uint8_t u8;
typedef uint32_t u32;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CLK_MHZ 160
#define SIGNAL_TIME_KHZ 800 // based on WS2812B datasheet https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf
// each bit lasts 1.25us (1250ns)
// one tick is 1/160mhz = 6.25ns 
// 160mhz / 0.8mhz = 200 ticks 

#define WS2812_PWM_0 64   //0.4us +- 150ns 0 high
#define WS2812_PWM_1 128 // 0.8us += 150ns 1 high

#define NUM_LEDS 3

#define RESET_SLOTS 60 // must be above 50us each slot is 200ticks that take 1.25us theferore 60 * 1.25us = 75us > 50
// R8 G8 B8
#define WS2812_BUF_LEN (NUM_LEDS * 24 + RESET_SLOTS)

#define T 3000 // time in millis given 3s

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static u16 dma_pwm_buffer[WS2812_BUF_LEN];
static u8 tx_ready = 1; // flag to check whether new colors may be sent

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
  void set_led(u8 idx,u8 r, u8 g, u8 b){
    u32 grb = ((u32)g << 16) | ((u32)r << 8) | (u32)b;
    int offset = idx * 24;

    for(int i = 0U; i < 24; i++){
      u8 current_bit = (grb >> (23 - i)) & 1;

      if(current_bit) dma_pwm_buffer[i + offset] = WS2812_PWM_1;
      else            dma_pwm_buffer[i + offset] = WS2812_PWM_0;
    }


  }

  void update(){
    if(tx_ready){
      tx_ready = 0;
      //https://controllerstech.com/pwm-in-stm32/
      //ambiguous info: some people say it is neccesary to use interrupt version of the function (ends with IT) to work with the callback although i cannot find it here
      HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (u32*)dma_pwm_buffer, WS2812_BUF_LEN);
    }
  }

  void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
  {
    if (htim->Instance == TIM3)
    {
      HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_1); 
      tx_ready = 1;                               
    }
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
  MX_DMA_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  u8 prev_btn_state = GPIO_PIN_SET; // pulled-up button on p13
  u8 blink_active = 0;              
  u32 blink_start = 0;  

  for (int i = NUM_LEDS * 24; i < WS2812_BUF_LEN; i++)
  {
    dma_pwm_buffer[i] = 0;
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    u32 now = HAL_GetTick();
    u32 cycle = now % T;

    u8 current_btn_state = HAL_GPIO_ReadPin(btn_GPIO_Port, btn_Pin);

    if (prev_btn_state == GPIO_PIN_SET && current_btn_state == GPIO_PIN_RESET)
    {
      if (!blink_active)
      {
        blink_active = 1; 
        blink_start = now;
      }
    }
    prev_btn_state = current_btn_state;

    u8 r = 0U;
    if(cycle <= 1500) r = 255;
    else r = 0;

    // linear 0-255 based on time
    u8 g = (u8)((cycle * 255) / T);

    u8 b = 0U;

    if (blink_active)
    {
      u32 elapsed = now - blink_start;
      if (elapsed < 900)
      {
        b = ((elapsed / 150) % 2 == 0) ? 255 : 0;
      }
      else
      {
        blink_active = 0;
        b = 0;
      }
    }

    static u32 last_update = 0;
    //avoiding HAL_DELAY as it blocks the processor
    if(now - last_update >= 16 && tx_ready){

      set_led(0, r, 0, 0);
      set_led(1, 0, g, 0);
      set_led(2, 0, 0, b);

      last_update = now;
      update();
    }



    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
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
