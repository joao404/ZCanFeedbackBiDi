/*********************************************************************
 * micros
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

#pragma once

// #include "stm32_def.h"
#include "main.h"

//#include "stm32yyxx_hal.h"
/**
 * @brief Initializes DWT_Cycle_Count for microsDelay function
 * @return Error DWT counter
 * 1: DWT counter Error
 * 0: DWT counter works
 */
uint32_t microsInit(void);
 
/**
 * @brief This function provides a delay (in microseconds)
 * @param microseconds: delay in microseconds
 */
static inline void microsDelay(volatile uint32_t microseconds)
{
  uint32_t clk_cycle_start = DWT->CYCCNT;
  // Go to number of cycles for system
  microseconds *= (HAL_RCC_GetHCLKFreq() / 1000000);
  // Delay till end
  while ((DWT->CYCCNT - clk_cycle_start) < microseconds);
}

/**
 * @brief This function provides a delay (in microseconds)
 * @param microseconds: delay in microseconds
 * @param clk_cycle_start: starttime in ticks from get_DWT_tick()
 */
static inline void microsDelay(volatile uint32_t microseconds, volatile uint32_t clk_cycle_start)
{
  // uint32_t clk_cycle_start = DWT->CYCCNT;
  // Go to number of cycles for system
  microseconds *= (HAL_RCC_GetHCLKFreq() / 1000000);
  // Delay till end
  while ((DWT->CYCCNT - clk_cycle_start) < microseconds);
}
 
/**
 * @brief This function provides the actual counter value (in microseconds)
 * @return counter value in microsecons
 */
static inline uint32_t micros(void)
{
  return ( DWT->CYCCNT / (HAL_RCC_GetHCLKFreq() / 1000000) );
}

/**
 * @brief This function provides the actual counter value
 * @return counter value in ticks
 */
static inline uint32_t get_DWT_tick(void)
{
  return DWT->CYCCNT;
}

