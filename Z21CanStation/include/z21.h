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
#include <DCCPacketScheduler.h>
#include <list>

constexpr uint32_t milliVoltagePerAmpere{185};
constexpr uint32_t offsetVoltageINmV{1940};
constexpr uint32_t resistorDivider{2};
constexpr uint32_t offsetINmV{offsetVoltageINmV / resistorDivider};

class z21 : public virtual ZCanInterfaceObserver, public virtual z21InterfaceObserver
{
public:
    struct ConfigLoco
    {
        uint16_t adr;
        uint8_t steps;
    };

    struct ConfigDccStation
    {
        uint8_t dccPin; // Pin for DCC sginal out
        uint8_t ndccPin;
        uint8_t enablePin1;
        uint8_t enablePin2;
        uint8_t shortPin; // Pin to detect Short Circuit
        uint16_t shortCircuitThresholdINmA;
    };

public:
    z21(ConfigDccStation &configDccStation, uint16_t hash, uint32_t serialNumber, HwType hwType, uint32_t swVersion, void (*printFunc)(const char *, ...) = nullptr, bool debugz21 = false, bool debugZ21 = false, bool debugZCan = false);
    virtual ~z21();
    void begin();

    void cyclic();

    void dcc();

    void update(Observable &observable, void *data) override;

    void deleteLocoConfig();

private:
    SemaphoreHandle_t m_xMutex {nullptr};

    Preferences m_preferences;

    const char *m_namespaceZ21{"z21"};

    const char *m_keyLocoMode{"locomode"};

    std::list<ConfigLoco> m_locos;

    DCCPacketScheduler m_dps;

    uint8_t m_powerState{0};

    uint8_t m_dccPin; // Pin for DCC sginal out
    uint8_t m_ndccPin;
    uint8_t m_enablePin1;
    uint8_t m_enablePin2;
    uint8_t m_shortPin; // Pin to detect Short Circuit

    uint16_t m_shortCircuitThresholdINmA;

    uint16_t m_currentINmA{0};

    // const uint32_t z21Uid{0xBADEAFFE};

    uint16_t m_serialNumber;

    bool m_debug;

    uint32_t m_lastPingSendTimeINms{0};

    void saveLocoConfig();

    uint16_t getSerialNumber() override;

    // onCallback
    void onIdenticalNetworkId() override;

    bool onRequestPing(uint16_t id) override;

    bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) override;

    bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value) override;

    bool onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value) override;

    // Z21
    void notifyz21InterfacegetSystemInfo(uint8_t client) override;

    void notifyz21InterfaceCANdetector(uint8_t client, uint8_t typ, uint16_t ID) override;

    void notifyz21InterfaceRailPower(EnergyState State) override;

    // void notifyz21InterfaceAccessoryInfo(uint16_t Adr, uint8_t &position) override;
    // void notifyz21InterfaceAccessory(uint16_t Adr, bool state, bool active) override;

    // // void notifyz21InterfaceExtAccessory(uint16_t Adr, byte state) override;

    void notifyz21InterfaceLocoState(uint16_t Adr, uint8_t data[]) override;
    void notifyz21InterfaceLocoFkt(uint16_t Adr, uint8_t type, uint8_t fkt) override;
    void notifyz21InterfaceLocoSpeed(uint16_t Adr, uint8_t speed, uint8_t stepConfig) override;

    void handleGetLocoMode(uint16_t adr, uint8_t &mode) override;
    void handleSetLocoMode(uint16_t adr, uint8_t mode) override;
    void handleGetTurnOutMode(uint16_t adr, uint8_t &mode) override;
    void handleSetTurnOutMode(uint16_t adr, uint8_t mode) override;
};