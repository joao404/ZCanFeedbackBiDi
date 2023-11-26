/*********************************************************************
 * CanInterfaceStm32
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you twai redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include "ZCan/CanInterfaceStm32.h"
#include <cstring>
#include <functional>

std::shared_ptr<CanInterfaceStm32> CanInterfaceStm32::m_instance;

CanInterfaceStm32::CanInterfaceStm32(bool useInterrupt, void (*printFunc)(const char *, ...))
    : m_usingInterrupt(useInterrupt)
{
    if (nullptr != printFunc)
    {
        m_printFunc = printFunc;
    }
}

CanInterfaceStm32::~CanInterfaceStm32()
{
}

std::shared_ptr<CanInterfaceStm32> CanInterfaceStm32::createInstance(bool useInterrupt, void (*printFunc)(const char *, ...))
{
    if (nullptr == CanInterfaceStm32::m_instance.get())
    {
        CanInterfaceStm32::m_instance = std::make_shared<CanInterfaceStm32>(useInterrupt, printFunc);
    }
    return CanInterfaceStm32::m_instance;
}

void CanInterfaceStm32::begin()
{
    m_printFunc("Setting up CAN GPIO...\n");

    m_canHandle.begin(Stm32Can::EXT_ID_LEN, (1 << 20) | (12 << 16) | (13 << 0), Stm32Can::PORTA_11_12_WIRE_PULLUP);

    if (m_usingInterrupt)
    {
        m_canHandle.attachInterrupt(CanInterfaceStm32::interruptHandler);
    }
}

void CanInterfaceStm32::cyclic()
{
    if (!m_usingInterrupt)
    {
        Can::Message frame{};
        if (receive(frame, 0))
        {
            notify(&frame);
        }
    }
    Can::Message frame;
    if (m_transmitQueue.front(frame))
    {
        if (m_canHandle.transmit(frame.identifier, &frame.data[0], frame.data_length_code))
        {
            m_transmitQueue.pop();
        }
    }
    errorHandling();
}

bool CanInterfaceStm32::transmit(Can::Message &frame, uint16_t timeoutINms)
{
    if (!m_canHandle.transmit(frame.identifier, &frame.data[0], frame.data_length_code))
    {

        // m_printFunc("Error TX\n");
        m_transmitQueue.emplace(frame);
    }
    return true;
}

bool CanInterfaceStm32::receive(Can::Message &frame, uint16_t timeoutINms)
{
    bool result{false};
    int id{0};
    int filterId{0};
    ;
    uint8_t data[8];
    uint32_t currentTimeINms = millis();
    do
    {
        frame.data_length_code = m_canHandle.receive(id, filterId, data);
        if (frame.data_length_code > -1) // poll for rx
        {
            frame.extd = 1;
            frame.identifier = id;
            memcpy(&frame.data[0], data, 8);
            result = true;
            break;
        }
    } while ((currentTimeINms + (uint32_t)timeoutINms) > millis());
    return result;
}

void CanInterfaceStm32::errorHandling()
{
}

void CanInterfaceStm32::interruptHandler()
{
    Can::Message frame{};
    if (CanInterfaceStm32::m_instance->receive(frame, 0))
    {
        CanInterfaceStm32::m_instance->notify(&frame);
    }
}