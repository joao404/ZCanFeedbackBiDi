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
#include "FeedbackDecoder/FeedbackDecoder.h"
#include <array>

class CurrentDecoder : public FeedbackDecoder
{
public:
    CurrentDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                   int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...) = nullptr,
                   bool debug = false, bool zcanDebug = false, bool currentDebug = false);
    virtual ~CurrentDecoder() override;

    virtual void callbackDccReceived() override;

    virtual void callbackLocoAddrReceived(uint16_t addr) override;

    virtual void callbackAdcReadFinished(ADC_HandleTypeDef *hadc) override;

protected:
    // configure input pins for feedback function
    void configInputs() override;

    void cyclicPortCheck() override;

    bool m_currentDebug;

    uint16_t *m_dmaBufferIN1samplePer1us;

    std::array<uint16_t, 128> m_adcDmaBufferCurrentSense;

    bool m_measurementCurrentSenseTriggered{false};

    bool m_measurementCurrentSenseRunning{false};

    bool m_measurementCurrentSenseProcessed{true};

    std::array<uint16_t, 512> m_adcDmaBufferRailcom;

    bool m_measurementRailcomTriggered{false};

    bool m_measurementRailcomRunning{false};

    bool m_measurementRailcomProcessed{true};
};
