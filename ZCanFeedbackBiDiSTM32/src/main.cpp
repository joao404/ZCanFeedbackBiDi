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
#include "stm32hal/main.h"
#include "stm32hal/usart.h"
#include "stm32hal/gpio.h"
#include "stm32hal/dma.h"
#include "stm32hal/adc.h"
#include "ZCan/CanInterfaceStm32.h"
#include "FeedbackDecoder.h"
#include "Helper/xprintf.h"
#include "Helper/micros.h"
#include <vector>
#include <memory>

CAN_HandleTypeDef hcan;

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

std::array<FeedbackDecoder::gpioPin, 8> trackPin1{{{GPIOA, GPIO_PIN_0},
                                                   {GPIOA, GPIO_PIN_1},
                                                   {GPIOA, GPIO_PIN_2},
                                                   {GPIOA, GPIO_PIN_3},
                                                   {GPIOA, GPIO_PIN_4},
                                                   {GPIOA, GPIO_PIN_5},
                                                   {GPIOA, GPIO_PIN_6},
                                                   {GPIOA, GPIO_PIN_7}}};

bool hasRailcom{false};

// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder1(memoryData.modulConfig1, flashWriteData, trackPin1, hasRailcom,
                                 {GPIOB, GPIO_PIN_0}, {GPIOB, GPIO_PIN_1}, true, true, xprintf);

int main(void)
{
  xdev_in(uart_getc);
  xdev_out(uart_putc);
  HAL_Init();
  SystemClock_Config();
  if(DWT_Delay_Init())
  {
    xprintf("Error DWT_Delay_Init()\n");
  }
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_DMA_Init();
  MX_ADC1_Init();

  xprintf("ZCAN 10808 Systemfrequenz: %lu\n", HAL_RCC_GetSysClockFreq());

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

  while (1)
  {
    //__NOP();
    canInterface->cyclic();
    feedbackDecoder1.cyclic();

    // Start ADC Conversion
    // Pass (The ADC Instance, Result Buffer Address, Buffer Length)
    // HAL_ADC_Start_DMA(&hadc1, AdcDmaBuffer, sizeof(AdcDmaBuffer)/4);

    // HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);
    // HAL_Delay(1000);

    uint8_t uartData[]{0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (HAL_OK == HAL_UART_Receive_IT(&huart1, uartData, sizeof(uartData) - 7))
    {
      xprintf("%s\n", uartData);
    }
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

  // uint64_t FData = 0x1A2B3C4D5E6F1234;		// Initialize the data to be written into the flash memory

  // HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,0x0801FC00, FData);  // Calling the HAL function to write the at data at the given memory address

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