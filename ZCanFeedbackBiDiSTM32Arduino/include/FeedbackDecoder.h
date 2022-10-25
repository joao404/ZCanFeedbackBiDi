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
#include "Railcom.h"

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

    enum class Detection:uint8_t
    {
        Digital=0,
        CurrentSense,
        Railcom
    };

    FeedbackDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin, Detection detectionConfig,
                    int configAnalogOffsetPin, int configIdPin, bool debug, bool zcanDebug, void (*printFunc)(const char *, ...) = nullptr);
    virtual ~FeedbackDecoder();

    void begin();

    void cyclic();

    void callbackAccAddrReceived(uint16_t addr);

    void callbackLocoAddrReceived(uint16_t addr);

    void callbackAdcReadFinished(ADC_HandleTypeDef *hadc);

protected:
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

    bool m_debug;

    bool (*m_saveDataFkt)(void);

    Detection m_detectionConfig;
    // if this ping is set to low during startup, offset values for analog measurements are taken
    int m_configAnalogOffsetPin;
    // ping for configuration of modul adress
    int m_configIdPin;

    uint16_t m_modulId;

    uint32_t m_idPrgStartTimeINms;

    bool m_idPrgRunning;

    uint32_t m_idPrgIntervalINms;

    uint32_t m_lastCanCmdSendINms;
    // jitter used for sending cyclic ping
    uint16_t m_pingJitterINms;
    // interval used for sending cyclic ping
    uint32_t m_pingIntervalINms;
    // id of current base station
    uint32_t m_masterId;

    uint16_t m_sessionId;
    // modul type of feedback decoder
    uint16_t m_modulType{roco10808Type};

    // adress is 0x8000 up tp 0xC000
    bool notifyLocoInBlock(uint8_t port, uint16_t adress1, uint16_t adress2, uint16_t adress3, uint16_t adress4);

    bool notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied);

    typedef struct
    {
        int pin;
        bool state;
        bool changeReported;
        std::array<uint16_t, 4> adress;
        uint16_t lastChannelData1;
        uint16_t lastChannelData2;
        uint16_t voltageOffset;
        unsigned long lastChangeTimeINms;
    } TrackData;

    std::array<TrackData, 8> m_trackData;

    uint8_t m_currentSensePort;

    uint8_t m_currentSenseMeasurement;

    const uint8_t m_currentSenseMeasurementMax;

    uint16_t m_currentSenseSum;

    uint8_t m_railcomDetectionPort;

    uint8_t m_railcomDetectionMeasurement;

    const uint8_t m_maxNumberOfConsecutiveMeasurements;

    std::array<uint16_t, 512> m_adcDmaBuffer;

    std::array<bool, 512> m_bitStreamDataBuffer;

    uint16_t m_trackSetVoltage;

    bool m_railcomCutOutActive;

    bool m_railcomDataProcessed;

private:
    ModulConfig &m_modulConfig;

    uint32_t m_firmwareVersion;
    uint32_t m_buildDate;
    uint32_t m_hardwareVersion;
};