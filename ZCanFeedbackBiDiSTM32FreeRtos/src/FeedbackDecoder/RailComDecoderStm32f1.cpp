/*********************************************************************
 * Railcom Decoder Stm32f1
 *
 * Copyright (C) 2023 Marcel Maage
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

#include "FeedbackDecoder/RailcomDecoderStm32f1.h"

RailcomDecoderStm32f1::RailcomDecoderStm32f1(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                               int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...),
                               bool debug, bool zcanDebug, bool railcomDebug)
    : RailcomDecoder(modulConfig, saveDataFkt, trackPin, configAnalogOffsetPin, configIdPin, statusLed, printFunc, debug, zcanDebug, railcomDebug)
{
}
RailcomDecoderStm32f1::~RailcomDecoderStm32f1()
{
}

void RailcomDecoderStm32f1::configAdcSingleMode()
{
    configAdcSingleMode();
}

void RailcomDecoderStm32f1::configAdcDmaMode()
{
    configContinuousDmaMode();
}

uint32_t RailcomDecoderStm32f1::singleAdcRead(int channel)
{
    setChannel(channel);
    // Start ADC Conversion
    HAL_ADC_Start(&hadc1);
    // Poll ADC1 Perihperal & TimeOut = 1mSec
    HAL_ADC_PollForConversion(&hadc1, 1);
    // Read The ADC Conversion Result & Map It To PWM DutyCycle
    return HAL_ADC_GetValue(&hadc1);
}

void RailcomDecoderStm32f1::triggerDmaRead(int channel, uint32_t *data, uint32_t length)
{
    setChannel(channel); // 4 us
    // start ADC conversion
    HAL_ADC_Start_DMA(&hadc1, data, length); // 26 us
}
