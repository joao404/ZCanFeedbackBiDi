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
#include <array>
#include "Stm32f1/adc.h"

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
        std::array<uint16_t, 8> voltageOffset;
    } ModulConfig;

    enum class Detection : uint8_t
    {
        Digital = 0,
        CurrentSense,
        Railcom
    };

    FeedbackDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                    int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...) = nullptr,
                    bool debug = false, bool zcanDebug = false);
    virtual ~FeedbackDecoder();

    virtual void begin();

    virtual void cyclic();

    virtual void callbackDccReceived();

    virtual void callbackAccAddrReceived(uint16_t addr);

    virtual void callbackLocoAddrReceived(uint16_t addr);

    virtual void callbackAdcReadFinished(ADC_HandleTypeDef *hadc);

protected:
    // configure input pins for feedback function
    virtual void configInputs();
    // reaction on a ZCan message with identical network id
    virtual void onIdenticalNetworkId() override;
    // reaction on Accessory Data message
    virtual bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type) override;
    // reaction on Accessory Port6 message
    virtual bool onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type) override;
    // reaction on request of modul info
    virtual bool onRequestModulInfo(uint16_t id, uint16_t type) override;
    // reaction on event of modul power info
    virtual bool onModulPowerInfoEvt(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA) override;
    // reaction on acknowledge of modul power info
    virtual bool onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA) override;

    virtual bool onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info) override;

    virtual bool onRequestModulObjectConfig(uint16_t id, uint32_t tag) override;

    virtual bool onCmdModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value) override;

    virtual bool onRequestPing(uint16_t id) override;

    virtual bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) override;

    bool sendMessage(ZCanMessage &message) override;

    ModulConfig &m_modulConfig;

    uint32_t m_firmwareVersion{0x05010014}; // 5.1.20
    uint32_t m_buildDate{0x07E60917};       // 23.09.2022
    uint32_t m_hardwareVersion{0x05010001}; // 5.1.1

    bool m_debug;

    bool (*m_saveDataFkt)(void);

    Detection m_detectionConfig;
    // if this ping is set to low during startup, offset values for analog measurements are taken
    int m_configAnalogOffsetPin;
    // ping for configuration of modul adress
    int m_configIdPin;

    uint16_t m_modulId{0};

    uint32_t m_idPrgStartTimeINms{0};

    bool m_idPrgRunning{false};

    uint32_t m_idPrgIntervalINms{60000}; // 1 min

    uint8_t &m_statusLed;

    uint32_t m_lastCanCmdSendINms{0};
    // jitter used for sending cyclic ping
    uint16_t m_pingJitterINms{0};
    // interval used for sending cyclic ping
    uint32_t m_pingIntervalINms{0};
    // id of current base station
    uint32_t m_masterId{0};

    uint16_t m_sessionId{0};
    // modul type of feedback decoder
    uint16_t m_modulType{roco10808Type};

    typedef struct
    {
        int pin;
        bool state;
        bool changeReported;
        uint16_t voltageOffset;
        uint32_t lastChangeTimeINms;
    } TrackData;

    std::array<TrackData, 8> m_trackData;

    uint16_t m_trackSetVoltage{0};

    uint8_t m_detectionPort{0};

    std::array<uint16_t, 128> m_adcDmaBufferCurrentSense;

    bool m_measurementCurrentSenseTriggered{false};

    bool m_measurementCurrentSenseRunning{false};

    bool m_measurementCurrentSenseProcessed{true};

    std::array<uint16_t, 512> m_adcDmaBufferRailcom;

    bool m_measurementRailcomTriggered{false};

    bool m_measurementRailcomRunning{false};

    bool m_measurementRailcomProcessed{true};

    bool notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied);

    void portStatusCheck(bool state, std::function<void(void)> callbackTrackSet, std::function<void(void)> callbackTrackReset);
};