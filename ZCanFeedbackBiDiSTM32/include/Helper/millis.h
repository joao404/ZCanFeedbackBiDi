/*********************************************************************
 * millis
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

#include "stm32hal/main.h"

volatile static uint32_t timerINms = 0;

void SysTick_Handler(void) {
  timerINms++;
}

uint32_t millis() {
  return timerINms;
}

void initMillis()
{
  // Initialise SysTick to tick at 1ms by initialising it with SystemCoreClock (Hz)/1000
  SysTick_Config(SystemCoreClock / 1000);
}