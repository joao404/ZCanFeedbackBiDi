
#pragma once

// #include "stm32_def.h"
#include "stm32hal/main.h"

//#include "stm32yyxx_hal.h"
/**
 * @brief Initializes DWT_Cycle_Count for DWT_Delay_us function
 * @return Error DWT counter
 * 1: DWT counter Error
 * 0: DWT counter works
 */
uint32_t DWT_Delay_Init(void);
 
/**
 * @brief This function provides a delay (in microseconds)
 * @param microseconds: delay in microseconds
 */
static inline void DWT_Delay_us(volatile uint32_t microseconds)
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
static inline void DWT_Delay_us(volatile uint32_t microseconds, volatile uint32_t clk_cycle_start)
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
static inline uint32_t get_DWT_us(void)
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

