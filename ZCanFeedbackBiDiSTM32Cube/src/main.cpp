/*********************************************************************
 * ZCanFeedbackBiDiSTM32
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "ZCan/CanInterfaceStm32.h"
#include "FeedbackDecoder.h"
#include "Helper/xprintf.h"
#include "Helper/micros.h"
#include "Dcc.h"
#include <vector>
#include <memory>
#include <stm32f1xx_hal_gpio.h>

void SystemClock_Config(void);

void uart_putc(uint8_t d)
{
  HAL_UART_Transmit(&huart1, &d, 1, HAL_MAX_DELAY);
}

uint8_t uart_getc(void)
{
  uint8_t d;

  (uint8_t) HAL_UART_Receive(&huart1, &d, 1, HAL_MAX_DELAY);

  return d;
}

#define START_ADDRESS_MEMORY 0x0800FC00

uint32_t AdcDmaBuffer[200];

std::shared_ptr<CanInterfaceStm32> canInterface = CanInterfaceStm32::createInstance(false, xprintf);

void flashReadData(void);
bool flashWriteData(void);

typedef struct
{
  FeedbackDecoder::ModulConfig modulConfig1;
  FeedbackDecoder::ModulConfig modulConfig2;
} MemoryData;

MemoryData memoryData;

std::array<FeedbackDecoder::gpioPin, 8> trackPin1{{{GPIOA, GPIO_PIN_0, ADC_CHANNEL_0},
                                                   {GPIOA, GPIO_PIN_1, ADC_CHANNEL_1},
                                                   {GPIOA, GPIO_PIN_2, ADC_CHANNEL_2},
                                                   {GPIOA, GPIO_PIN_3, ADC_CHANNEL_3},
                                                   {GPIOA, GPIO_PIN_4, ADC_CHANNEL_4},
                                                   {GPIOA, GPIO_PIN_5, ADC_CHANNEL_5},
                                                   {GPIOA, GPIO_PIN_6, ADC_CHANNEL_6},
                                                   {GPIOA, GPIO_PIN_7, ADC_CHANNEL_7}}};

bool hasRailcom{true};

// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder1(memoryData.modulConfig1, flashWriteData, trackPin1, hasRailcom,
                                 {configRailcom_GPIO_Port, configRailcom_Pin}, {configNetworkId_GPIO_Port, configNetworkId_Pin}, true, false, xprintf);

// Called when first half of buffer is filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_RESET);
  feedbackDecoder1.callbackAdcReadFinished(hadc);
}

Dcc dcc([]() -> bool
        { return (HAL_GPIO_ReadPin(dccInput_GPIO_Port, dccInput_Pin) == GPIO_PinState::GPIO_PIN_SET); });

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == dccInput_Pin)
  {
    dcc.interruptHandler();
  }
}

void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower)
{
  xprintf("notifyDccAccTurnoutOutput: %u\n", Addr);
  feedbackDecoder1.callbackAccAddrReceived(Addr);
}

void notifyDccLoco(uint16_t addr, Dcc::AddrType addrType)
{
  xprintf("notifyDccLoco: %u\n", addr);
  feedbackDecoder1.callbackLocoAddrReceived(addr);
}

int main(void)
{
  xdev_in(uart_getc);
  xdev_out(uart_putc);
  HAL_Init();
  SystemClock_Config();
  if (microsInit())
  {
    xprintf("Error microsInit()\n");
  }
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  xprintf("ZCAN Feedback Decoder system frequency: %lu\n", HAL_RCC_GetSysClockFreq());
  // Calibrate The ADC On Power-Up For Better Accuracy
  HAL_ADCEx_Calibration_Start(&hadc1);

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  if (!feedbackDecoder1.setCanObserver(canInterface))
  {
    xprintf("ERROR: No can interface defined\n");
  }

  flashReadData();

  feedbackDecoder1.begin();

  dcc.setNotifyDccAccTurnoutOutput(notifyDccAccTurnoutOutput);
  dcc.setNotifyDccLoco(notifyDccLoco);

  dcc.begin();

  while (1)
  {
    static unsigned long lastBlinkTimeINms = 0;

    canInterface->cyclic();
    feedbackDecoder1.cyclic();
    unsigned long currentTimeINms{HAL_GetTick()};
    if ((lastBlinkTimeINms + 1000) < currentTimeINms)
    {
      // HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);
      lastBlinkTimeINms = currentTimeINms;
    }

    dcc.cyclic();

    // Start ADC Conversion
    // Pass (The ADC Instance, Result Buffer Address, Buffer Length)
    // HAL_ADC_Start_DMA(&hadc1, AdcDmaBuffer, sizeof(AdcDmaBuffer)/4);

    // HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);
    // HAL_Delay(1000);

    // uint8_t uartData[]{0, 0, 0, 0, 0, 0, 0, 0, 0};
    // if (HAL_OK == HAL_UART_Receive_IT(&huart1, uartData, sizeof(uartData) - 7))
    // {
    //   xprintf("%s\n", uartData);
    // }
  }
}

void flashReadData(void)
{
  uint16_t *memoryPtr = (uint16_t *)START_ADDRESS_MEMORY;
  uint16_t *dataPtr = (uint16_t *)(&memoryData);
  for (uint16_t i = 0; i < sizeof(memoryData); i += 2)
  {
    *dataPtr = *(__IO uint16_t *)memoryPtr;
    memoryPtr++;
    dataPtr++;
  }
}

bool flashWriteData(void)
{

  static FLASH_EraseInitTypeDef eraseInitStruct{0};

  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  /* Erase the user Flash area */

  /* Fill EraseInit structure*/
  eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  eraseInitStruct.PageAddress = START_ADDRESS_MEMORY; // Starting address of flash page (0x0800 0000 - 0x0801 FC00)
  eraseInitStruct.NbPages = 1;                        // The number of pages to be erased

  uint32_t errorStatus = 0;

  if (HAL_FLASHEx_Erase(&eraseInitStruct, &errorStatus) != HAL_OK)
  {
    xprintf("%d\n", HAL_FLASH_GetError());
    return false;
  }

  uint32_t memoryAdress = START_ADDRESS_MEMORY;
  uint16_t *dataPtr = (uint16_t *)(&memoryData);
  for (uint16_t i = 0; i < sizeof(memoryData); i += 2)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, memoryAdress, *dataPtr) == HAL_OK)
    {
      memoryAdress += 2; // use StartPageAddress += 2 for half word and 8 for double word
      dataPtr++;
    }
    else
    {
      /* Error occurred while writing data in Flash memory*/
      xprintf("%d\n", HAL_FLASH_GetError());
      return false;
    }
  }

  /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
  HAL_FLASH_Lock();
  return true;
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
   */
  HAL_RCC_EnableCSS();
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
