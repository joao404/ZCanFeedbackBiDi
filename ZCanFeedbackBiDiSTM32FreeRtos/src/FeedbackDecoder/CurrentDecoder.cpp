/*********************************************************************
 * Current Decoder
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

#include "FeedbackDecoder/CurrentDecoder.h"

CurrentDecoder::CurrentDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                               int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...),
                               bool debug, bool zcanDebug, bool currentDebug)
    : FeedbackDecoder(modulConfig, saveDataFkt, trackPin, configAnalogOffsetPin, configIdPin, statusLed, printFunc, debug, zcanDebug),
      m_currentDebug(currentDebug)
{
}
CurrentDecoder::~CurrentDecoder()
{
}

void CurrentDecoder::configInputs()
{
    pinMode(m_configAnalogOffsetPin, INPUT_PULLUP);
    //  this is done outside of FeedbackDecoder
    if (!digitalRead(m_configAnalogOffsetPin))
    {
        ZCanInterfaceObserver::m_printFunc("Offset measuring\n");
        configSingleMeasurementMode();
        // read current values of adcs as default value
        for (uint8_t port = 0; port < m_trackData.size(); ++port)
        {
            setChannel(m_trackData[port].pin);
            // Start ADC Conversion
            HAL_ADC_Start(&hadc1);
            // Poll ADC1 Perihperal & TimeOut = 1mSec
            HAL_ADC_PollForConversion(&hadc1, 1);
            // Read The ADC Conversion Result & Map It To PWM DutyCycle
            m_trackData[port].voltageOffset = HAL_ADC_GetValue(&hadc1);
            m_modulConfig.voltageOffset[port] = m_trackData[port].voltageOffset;
            ZCanInterfaceObserver::m_printFunc("Offset measurement port %d: %d\n", port, m_modulConfig.voltageOffset[port]);
        }
        m_saveDataFkt();
    }
    else
    {
        for (uint8_t port = 0; port < m_trackData.size(); ++port)
        {
            m_trackData[port].voltageOffset = m_modulConfig.voltageOffset[port];
        }
    }
    configContinuousDmaMode();                    // 6us
    setChannel(m_trackData[m_detectionPort].pin); // 4us
}

void CurrentDecoder::cyclicPortCheck()
{
    if (m_measurementCurrentSenseTriggered && !m_measurementCurrentSenseRunning && !m_measurementCurrentSenseProcessed)
    {
        uint16_t m_currentSenseSum{0};
        for (uint16_t &measurement : m_adcDmaBufferCurrentSense)
        {
            if (measurement > m_trackData[m_detectionPort].voltageOffset)
            {
                m_currentSenseSum += (measurement - m_trackData[m_detectionPort].voltageOffset);
            }
            else
            {
                m_currentSenseSum += (m_trackData[m_detectionPort].voltageOffset - measurement);
            }
        }
        m_currentSenseSum /= m_adcDmaBufferCurrentSense.size();
        bool state = m_currentSenseSum > m_trackSetVoltage;
        checkPortStatusChange(state);
        m_detectionPort++;
        if (m_trackData.size() > m_detectionPort)
        {
            m_measurementCurrentSenseRunning = true;
            setChannel(m_trackData[m_detectionPort].pin);
            // start ADC conversion
            HAL_ADC_Start_DMA(&hadc1, (uint32_t *)m_adcDmaBufferCurrentSense.begin(), m_adcDmaBufferCurrentSense.size()); // 26 us
        }
        else
        {
            // no more measurements
            m_measurementCurrentSenseTriggered = false;
        }
        m_measurementCurrentSenseProcessed = true;
    }
    ///////////////////////////////////////////////////////////////////////////
    // process Railcom data from ADC
    if (m_measurementRailcomTriggered && !m_measurementRailcomRunning)
    {
        if (!m_measurementRailcomProcessed)
        {
            m_measurementRailcomProcessed = true;
            m_measurementRailcomTriggered = false;
            //  trigger measurement of current sense
            HAL_ADC_Start_DMA(&hadc1, (uint32_t *)m_adcDmaBufferCurrentSense.begin(), m_adcDmaBufferCurrentSense.size()); // 26 us
            m_measurementCurrentSenseTriggered = true;
            m_measurementCurrentSenseRunning = true;
        }
    }
}

void CurrentDecoder::callbackDccReceived()
{
    if (!m_measurementRailcomRunning)
    {
        m_measurementRailcomTriggered = true;
        m_measurementRailcomRunning = true;
        m_detectionPort = 0;
        setChannel(m_trackData[m_detectionPort].pin);                                                       // 4us
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)m_adcDmaBufferRailcom.begin(), m_adcDmaBufferRailcom.size()); // 26 us
    }
}

void CurrentDecoder::callbackLocoAddrReceived(uint16_t addr)
{
}

void CurrentDecoder::callbackAdcReadFinished(ADC_HandleTypeDef *hadc)
{
    if (m_measurementCurrentSenseTriggered)
    {
        m_measurementCurrentSenseRunning = false;
        m_measurementCurrentSenseProcessed = false;
    }
    if (m_measurementRailcomTriggered)
    {
        m_measurementRailcomRunning = false;
        m_measurementRailcomProcessed = false;
    }
}