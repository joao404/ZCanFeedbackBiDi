/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_BUILTIN_Pin GPIO_PIN_13
#define LED_BUILTIN_GPIO_Port GPIOC
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
#define configRailcom_Pin GPIO_PIN_0
#define configRailcom_GPIO_Port GPIOB
#define configNetworkId_Pin GPIO_PIN_1
#define configNetworkId_GPIO_Port GPIOB
#define dccInput_Pin GPIO_PIN_8
#define dccInput_GPIO_Port GPIOA
#define dccInput_EXTI_IRQn EXTI9_5_IRQn
#define usartTx_Pin GPIO_PIN_9
#define usartTx_GPIO_Port GPIOA
#define usartRx_Pin GPIO_PIN_10
#define usartRx_GPIO_Port GPIOA
#define canRx_Pin GPIO_PIN_11
#define canRx_GPIO_Port GPIOA
#define canTx_Pin GPIO_PIN_12
#define canTx_GPIO_Port GPIOA
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
