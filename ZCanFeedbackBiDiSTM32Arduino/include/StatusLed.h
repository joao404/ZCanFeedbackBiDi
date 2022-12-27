/*********************************************************************
 * StatusLed
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

#include "Arduino.h"

template <std::size_t N>
class StatusLed
{
public:
    StatusLed(int clkPin, int dataPin, int triggerOutputPin, int resetPin, uint32_t bitTimingINus,
              void (*printFunc)(const char *, ...) = nullptr, bool debug = false);
    virtual ~StatusLed();

    void cyclic();

    std::array<uint8_t, N> &getStatusArray();

protected:
    bool m_debug;

    void (*m_printFunc)(const char *, ...);

    int m_clkPin;
    int m_dataPin;
    int m_triggerOutputPin;
    int m_resetPin;

    uint32_t m_bitTimingINus;

    uint32_t m_lastShiftTimeINus{0};

    std::array<uint8_t, N> m_statusArray;

    uint8_t m_bitCount{0};
    uint8_t m_byteCount{0};

    enum class ShiftState : uint8_t
    {
        eData,
        eClkHigh,
        eClkLow,
        eOutHigh,
        eOutLow
    };
    ShiftState m_shiftState{ShiftState::eData};
};

template <std::size_t N>
StatusLed<N>::StatusLed(int clkPin, int dataPin, int triggerOutputPin, int resetPin, uint32_t bitTimingINus,
                        void (*printFunc)(const char *, ...), bool debug)
    : m_debug(debug),
      m_printFunc(printFunc),
      m_clkPin(clkPin),
      m_dataPin(dataPin),
      m_triggerOutputPin(triggerOutputPin),
      m_resetPin(resetPin),
      m_bitTimingINus(bitTimingINus)
{
    pinMode(m_clkPin, OUTPUT);
    pinMode(m_dataPin, OUTPUT);
    pinMode(m_triggerOutputPin, OUTPUT);
    pinMode(m_resetPin, OUTPUT);
    digitalWrite(m_clkPin, LOW);
    digitalWrite(m_dataPin, LOW);
    digitalWrite(m_triggerOutputPin, LOW);
    digitalWrite(m_resetPin, LOW);
}

template <std::size_t N>
StatusLed<N>::~StatusLed()
{
}

template <std::size_t N>
void StatusLed<N>::cyclic()
{
    unsigned long currentTimeINus{micros()};
    if ((m_lastShiftTimeINus + m_bitTimingINus) < currentTimeINus)
    {
        switch (m_shiftState)
        {
        case ShiftState::eData:
            digitalWrite(m_dataPin, bitRead(m_statusArray[m_byteCount], m_bitCount) ? HIGH : LOW);
            m_shiftState = ShiftState::eClkHigh;
            break;
        case ShiftState::eClkHigh:
            digitalWrite(m_clkPin, HIGH);
            m_shiftState = ShiftState::eClkLow;
            break;
        case ShiftState::eClkLow:
            digitalWrite(m_clkPin, LOW);
            m_bitCount++;
            if (8 == m_bitCount)
            {
                m_bitCount = 0;
                m_byteCount++;
                if (m_statusArray.size() == m_byteCount)
                {
                    m_byteCount = 0;
                    m_shiftState = ShiftState::eOutHigh;
                }
            }
            m_shiftState = ShiftState::eData;
            break;
        case ShiftState::eOutHigh:
            digitalWrite(m_triggerOutputPin, HIGH);
            m_shiftState = ShiftState::eOutLow;
            break;
        case ShiftState::eOutLow:
            digitalWrite(m_triggerOutputPin, LOW);
            m_shiftState = ShiftState::eData;
            break;
        default:
            digitalWrite(m_triggerOutputPin, LOW);
            digitalWrite(m_clkPin, LOW);
            m_shiftState = ShiftState::eData;
        };
        m_lastShiftTimeINus = currentTimeINus;
    }
}

template <std::size_t N>
std::array<uint8_t, N> &StatusLed<N>::getStatusArray()
{
    return m_statusArray;
}