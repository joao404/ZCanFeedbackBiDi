/*********************************************************************
 * CanInterfaceStm32
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

#include <memory>
#include <queue>
#include "ZCan/CanInterface.h"
#include "Stm32f1/Stm32Can.h"
#include "Helper/FiFo.h"

class CanInterfaceStm32 : public CanInterface
{
public:
    CanInterfaceStm32(bool useInterrupt, void (*printFunc)(const char *, ...) = nullptr);
    
    virtual ~CanInterfaceStm32();

    static std::shared_ptr<CanInterfaceStm32> createInstance(bool useInterrupt, void (*printFunc)(const char *, ...));

    void begin() override;

    void cyclic();

    bool transmit(Can::Message &frame, uint16_t timeoutINms) override;

    bool receive(Can::Message &frame, uint16_t timeoutINms) override;

    static void interruptHandler();

private:
    static std::shared_ptr<CanInterfaceStm32> m_instance;

    Stm32Can m_canHandle {};

    bool m_usingInterrupt;

    FiFo<Can::Message, 50> m_transmitQueue;

    void errorHandling();

    void (*m_printFunc)(const char *, ...){};
};