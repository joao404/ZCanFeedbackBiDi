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
#include "Arduino.h"
#include "ZCan/CanInterfaceStm32.h"
#include "FeedbackDecoder.h"
#include "Helper/xprintf.h"
#include "NmraDcc.h"
#include <vector>
#include <memory>

void uart_putc(uint8_t d)
{
  Serial.print((char)d);
}

#define START_ADDRESS_MEMORY 0x0800FC00

std::shared_ptr<CanInterfaceStm32> canInterface = CanInterfaceStm32::createInstance(false, xprintf);

void flashReadData(void);
bool flashWriteData(void);

typedef struct
{
  FeedbackDecoder::ModulConfig modulConfig1;
  FeedbackDecoder::ModulConfig modulConfig2;
} MemoryData;

MemoryData memoryData;

std::array<int, 8> trackPin1{PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

bool hasRailcom{false};
int configRailcomPin = PB0;
int configIdPin = PB1;

// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder1(memoryData.modulConfig1, flashWriteData, trackPin1, hasRailcom,
                                 configRailcomPin, configIdPin, true, false, xprintf);

// Called when first half of buffer is filled
// void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
// {
//   HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);
// }

// // Called when buffer is completely filled
// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
// {
//   HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_RESET);
//   feedbackDecoder1.callbackAdcReadFinished(hadc);
// }

NmraDcc dcc;
DCC_MSG Packet;

void setup()
{
  Serial.begin(115200);
  xdev_out(uart_putc);
  Serial.printf("ZCAN Feedback Decoder system frequency: %lu\n", HAL_RCC_GetSysClockFreq());
  // Calibrate The ADC On Power-Up For Better Accuracy
  // HAL_ADCEx_Calibration_Start(&hadc1);

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  delay(10);

  if (!feedbackDecoder1.setCanObserver(canInterface))
  {
    xprintf("ERROR: No can interface defined\n");
  }

  flashReadData();

  feedbackDecoder1.begin();

  dcc.pin(PA8, 0);
  dcc.init(MAN_ID_DIY, 10, 0, 0);

  Serial.println("Finished config");
}

void loop()
{
  dcc.process();
  canInterface->cyclic();
  feedbackDecoder1.cyclic();
}

void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower)
{
  // Serial.printf("notifyDccAccTurnoutOutput: %u\n", Addr);
  feedbackDecoder1.callbackAccAddrReceived(Addr);
}

void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{
  // Serial.printf("notifyDccFunc: %u\n", Addr);
  feedbackDecoder1.callbackLocoAddrReceived(Addr);
}

void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps)
{
  // Serial.printf("notifyDccSpeed: %u\n", Addr);
  feedbackDecoder1.callbackLocoAddrReceived(Addr);
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

extern "C" void SystemClock_Config(void)
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