/*********************************************************************
 * z21
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

#include "ZCan/ZCanInterfaceObserver.h"
#include "z21/z21InterfaceObserver.h"
#include "Preferences.h"

class z21 : public virtual ZCanInterfaceObserver, public virtual z21InterfaceObserver
{
public:
    z21(uint16_t hash, uint32_t serialNumber, HwType hwType, uint32_t swVersion, void (*printFunc)(const char *, ...) = nullptr, bool debugz21 = false, bool debugZ21 = false, bool debugZCan = false);
    virtual ~z21();
    void begin();

    void cyclic();

    void update(Observable &observable, void *data) override;

private:
    // const uint32_t z21Uid{0xBADEAFFE};

    uint16_t m_serialNumber;

    bool m_debug;

    uint32_t m_lastPingSendTimeINms{0};

    uint16_t getSerialNumber() override;

    // onCallback
    void onIdenticalNetworkId() override;

    bool onRequestPing(uint16_t id) override;

    bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) override;

    bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value) override;

    bool onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value) override;

    // Z21
    // void notifyz21InterfacegetSystemInfo(uint8_t client) override;

    void notifyz21InterfaceCANdetector(uint8_t client, uint8_t typ, uint16_t ID) override;

    // void notifyz21InterfaceRailPower(EnergyState State) override;

    // void notifyz21InterfaceAccessoryInfo(uint16_t Adr, uint8_t &position) override;
    // void notifyz21InterfaceAccessory(uint16_t Adr, bool state, bool active) override;

    // // void notifyz21InterfaceExtAccessory(uint16_t Adr, byte state) override;

    // void notifyz21InterfaceLocoState(uint16_t Adr, uint8_t data[]) override;
    // void notifyz21InterfaceLocoFkt(uint16_t Adr, uint8_t type, uint8_t fkt) override;
    // void notifyz21InterfaceLocoSpeed(uint16_t Adr, uint8_t speed, uint8_t stepConfig) override;
};