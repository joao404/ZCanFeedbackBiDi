/*********************************************************************
 * Feedback Decoder
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
#include "main.h"
#include <array>

class FeedbackDecoder : public ZCanInterfaceObserver
{
public:
    // every variable must have size of 16 bits to fit memory layout
    typedef struct
    {
        uint16_t trackSetCurrentINmA;
        uint16_t trackFreeToSetTimeINms;
        uint16_t trackSetToFreeTimeINms;
    } TrackConfig;

    typedef struct
    {
        uint16_t networkId;
        uint16_t modulAdress;
        uint16_t sendChannel2Data;
        // std::array<TrackConfig, 8> trackConfig;
        TrackConfig trackConfig;
    } ModulConfig;

    typedef struct 
    {
        GPIO_TypeDef *bank;
        uint16_t pin;
    } gpioPin;

    FeedbackDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<gpioPin, 8> &trackPin, bool hasRailcom,
                    gpioPin configRailcomPin, gpioPin configIdPin, bool debug, bool zcanDebug, void (*printFunc)(const char *, ...) = nullptr);
    virtual ~FeedbackDecoder();

    void begin();

    void cyclic();

    void callbackAccAddrReceived(uint16_t addr);

    void callbackLocoAddrReceived(uint16_t addr);

protected:
    virtual void onIdenticalNetworkId() override;

    virtual bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type) override;

    virtual bool onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type) override;

    virtual bool onRequestModulInfo(uint16_t id, uint16_t type) override;

    virtual bool onModulPowerInfoEvt(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA) override;

    virtual bool onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA) override;

    virtual bool onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info) override;

    virtual bool onRequestModulObjectConfig(uint16_t id, uint32_t tag) override;

    virtual bool onCmdModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value) override;

    virtual bool onRequestPing(uint16_t id) override;

    virtual bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) override;

    bool sendMessage(ZCanMessage &message) override;

    bool m_debug;

    bool (*m_saveDataFkt)(void);

    bool m_hasRailcom;

    gpioPin m_configRailcomPin;

    gpioPin m_configIdPin;

    uint16_t m_modulId;

    uint32_t m_idPrgStartTimeINms;

    bool m_idPrgRunning;

    uint32_t m_idPrgIntervalINms;

    uint32_t m_lastCanCmdSendINms;

    uint16_t m_pingJitterINms;

    uint32_t m_pingIntervalINms;

    uint32_t m_masterId;

    uint16_t m_sessionId;

    uint16_t m_modulType{roco10808Type};

    // adress is 0x8000 up tp 0xC000
    bool notifyLocoInBlock(uint8_t port, uint16_t adress1, uint16_t adress2, uint16_t adress3, uint16_t adress4);

    bool notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied);

    typedef struct
    {
        gpioPin pin;
        bool state;
        bool changeReported;
        std::array<uint16_t, 4> adress;
        unsigned long lastChangeTimeINms;
    } TrackData;

    std::array<TrackData, 8> m_trackData;

private:
    ModulConfig &m_modulConfig;

    uint32_t m_firmwareVersion;
    uint32_t m_buildDate;
    uint32_t m_hardwareVersion;
};