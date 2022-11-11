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
#include "Stm32f1/Flash.h"
#include <vector>
#include <memory>
#include "Stm32f1/adc.h"
#include "Stm32f1/dma.h"

void uart_putc(uint8_t d)
{
  Serial.print((char)d);
}

std::shared_ptr<CanInterfaceStm32> canInterface = CanInterfaceStm32::createInstance(false, xprintf);

typedef struct
{
  FeedbackDecoder::ModulConfig modulConfig1;
  FeedbackDecoder::ModulConfig modulConfig2;
} MemoryData;

MemoryData memoryData;

std::array<int, 8> trackPin1{PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

FeedbackDecoder::Detection detectionMode1{FeedbackDecoder::Detection::Railcom};
int configRailcomPin{PB0};
int configIdPin1{PB1};

// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder1(memoryData.modulConfig1, Flash::writeData, trackPin1, detectionMode1,
                                 configRailcomPin, configIdPin1, true, true, true, xprintf);

// std::array<int, 8> trackPin2{PA15, PB3, PB4, PB5, PB6, PB7, PB8, PB9};

// FeedbackDecoder::Detection detectionMode2{FeedbackDecoder::Detection::Digital};
// int configIdPin2{PB10};

// // I will need in the end two of those moduls to handle each of the 8 inputs
// FeedbackDecoder feedbackDecoder2(memoryData.modulConfig2, Flash::writeData, trackPin2, detectionMode2,
//                                  configRailcomPin, configIdPin2, true, true, xprintf);

int ledPin{PC13};
uint32_t lastLedBlinkINms{0};
uint32_t ledBlinkIntervalINms{1000};

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  feedbackDecoder1.callbackAdcReadFinished(hadc);
}

NmraDcc dcc;
DCC_MSG Packet;

void setup()
{
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  xdev_out(uart_putc);
  MX_DMA_Init();
  MX_ADC1_Init();
  Serial.printf("ZCAN Feedback Decoder system frequency: %lu\n", HAL_RCC_GetSysClockFreq());
  // Calibrate The ADC On Power-Up For Better Accuracy
  HAL_ADCEx_Calibration_Start(&hadc1);

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  delay(10);

  if (!feedbackDecoder1.setCanObserver(canInterface))
  {
    xprintf("ERROR: No can interface for decoder 1 defined\n");
  }

  // if (!feedbackDecoder2.setCanObserver(canInterface))
  // {
  //   xprintf("ERROR: No can interface for decoder 2 defined\n");
  // }

  Flash::m_memoryDataPtr = (uint16_t *)&memoryData;
  Flash::m_memoryDataSize = sizeof(MemoryData);
  Flash::readData();

  feedbackDecoder1.begin();
  // feedbackDecoder2.begin();

  dcc.pin(PA8, 0);
  dcc.init(MAN_ID_DIY, 10, 0, 0);

  Serial.println("Finished config");
}

void loop()
{
  dcc.process();
  canInterface->cyclic();
  feedbackDecoder1.cyclic();
  // feedbackDecoder2.cyclic();
  uint32_t currentTimeINms = millis();
  if ((currentTimeINms - lastLedBlinkINms) > ledBlinkIntervalINms)
  {
    digitalToggle(ledPin);
    lastLedBlinkINms = currentTimeINms;
  }
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