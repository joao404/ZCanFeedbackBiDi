/*********************************************************************
 * Railcom Decoder
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

#pragma once

#include "Arduino.h"
#include "FeedbackDecoder/RailcomDecoder.h"

class RailcomDecoderStm32f1 : public RailcomDecoder
{
public:
    RailcomDecoderStm32f1(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                          int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...) = nullptr,
                          bool debug = false, bool zcanDebug = false, bool railcomDebug = false);
    virtual ~RailcomDecoderStm32f1() override;

protected:
    void configAdcSingleMode() override;

    void configAdcDmaMode() override;

    uint32_t singleAdcRead(int channel) override;

    void triggerDmaRead(int channel, uint32_t *data, uint32_t length) override;
};