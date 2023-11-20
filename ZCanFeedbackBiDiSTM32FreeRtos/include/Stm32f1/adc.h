/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#define Track1_Pin GPIO_PIN_0
#define Track1_GPIO_Port GPIOA
#define Track2_Pin GPIO_PIN_1
#define Track2_GPIO_Port GPIOA
#define Track3_Pin GPIO_PIN_2
#define Track3_GPIO_Port GPIOA
#define Track4_Pin GPIO_PIN_3
#define Track4_GPIO_Port GPIOA
#define Track5_Pin GPIO_PIN_4
#define Track5_GPIO_Port GPIOA
#define Track6_Pin GPIO_PIN_5
#define Track6_GPIO_Port GPIOA
#define Track7_Pin GPIO_PIN_6
#define Track7_GPIO_Port GPIOA
#define Track8_Pin GPIO_PIN_7
#define Track8_GPIO_Port GPIOA

const uint32_t channel[8]={ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};
/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */

void DMA1_Channel1_IRQHandler(void);

void configSingleMeasurementMode();

void configContinuousDmaMode();

void setChannel(int pin);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

