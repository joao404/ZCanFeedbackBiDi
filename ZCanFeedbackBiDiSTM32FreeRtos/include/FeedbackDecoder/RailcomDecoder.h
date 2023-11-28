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

#include <cstdint>
#include <array>
#include "FeedbackDecoder/FeedbackDecoder.h"
#include "FeedbackDecoder/Railcom/Packet.h"
#include <STM32FreeRTOS.h>

class RailcomDecoder : public FeedbackDecoder
{
public:
    RailcomDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                   int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...) = nullptr,
                   bool debug = false, bool zcanDebug = false, bool railcomDebug = false);
    virtual ~RailcomDecoder() override;

    virtual void callbackDccReceived() override;

    virtual void callbackAccAddrReceived(uint16_t addr) override;

    virtual void callbackLocoAddrReceived(uint16_t addr) override;

    virtual void callbackAdcReadFinished(ADC_HandleTypeDef *hadc) override;

protected:
    typedef struct
    {
        uint16_t address{0};
        uint16_t direction{0};
        uint32_t lastChangeTimeINms{0};
        bool changeReported{true};
    } RailcomAddr;

    typedef struct RailcomData
    {
        std::array<RailcomAddr, 4> railcomAddr;
        std::array<RailcomPacket12Bit, 2> channel1Data;
    } RailcomData;

    enum class Channel : uint8_t
    {
        eUnknown,
        eChannel1,
        eChannel2
    };

    typedef struct RailcomByte
    {
        uint8_t data;
        int8_t direction;
        size_t startIndex;
        size_t endIndex;
        bool valid;

        RailcomByte() : data(0xFF),
                        direction(0),
                        startIndex(1),
                        endIndex(0),
                        valid(false){

                        };
    } RailcomByte;

    typedef struct RailcomChannelData
    {
        size_t size;
        std::array<RailcomByte, 8> bytes;
        RailcomChannelData() : size(0){

                               };
    } RailcomChannelData;

    enum class AddressType : uint8_t
    {
        eNone,
        eLoco,
        eAcc
    };

    virtual void configAdcSingleMode() = 0;

    virtual void configAdcDmaMode() = 0;

    virtual uint32_t singleAdcRead(int channel) = 0;

    virtual void triggerDmaRead(int channel, uint32_t *data, uint32_t length) = 0;

    // configure input pins for feedback function
    void configInputs() override;

    void cyclicPortCheck() override;

    void onBlockOccupied() override;

    void onBlockEmpty(size_t blockNum) override;
    // reaction on Accessory Data message
    virtual bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type) override;

    void analyzeRailcomData(uint16_t dmaBufferIN1samplePer1us[], size_t length, uint16_t voltageOffset, uint16_t trackSetVoltage);

    bool getStartAndStopByteOfUart(bool *bitStreamIN1samplePer1us, size_t startIndex, size_t endIndex, size_t *findStartIndex, size_t *findEndIndex);

    void handleBitStream(uint16_t dmaBufferIN1samplePer1us[], size_t length, RailcomChannelData &channel1, RailcomChannelData &channel2, uint16_t voltageOffset, uint16_t trackSetVoltage);

    void handleFoundLocoAddr(uint16_t locoAddr, uint16_t direction, Channel channel, std::array<uint16_t, 4> &railcomData);

    void checkRailcomDataChange(RailcomAddr &data);

    // address is 0x8000 up tp 0xC000
    bool notifyLocoInBlock(uint8_t port, std::array<RailcomAddr, 4> railcomAddr);

    bool m_railcomDebug{false};

    std::array<uint16_t, 128> m_adcDmaBufferCurrentSense;
    
    SemaphoreHandle_t m_currentSenseDataReady;
    SemaphoreHandle_t m_railcomSenseDataReady;

    bool m_currentSenseRunning{false};
    bool m_railcomSenseRunning{false};

    std::array<uint16_t, 512> m_adcDmaBufferRailcom;


    uint16_t *m_dmaBufferIN1samplePer1us;

    uint16_t m_lastRailcomAddress{0};

    AddressType m_addrReceived{AddressType::eNone};

    uint32_t m_railcomDataTimeoutINms{2000};

    uint32_t m_railcomDataChangeCycleINms{100};

    std::array<RailcomData, 8> m_railcomData;

    uint8_t m_railcomDetectionPort{0};

    uint8_t m_cyclicRailcomCheckPort{0};

    uint8_t m_railcomDetectionMeasurement{0};

    const uint8_t m_maxNumberOfConsecutiveMeasurements{3};

    uint16_t m_channel1Direction{0};

    uint16_t m_channel2Direction{0};

    static uint8_t encode4to8[];
    static uint8_t encode8to4[];
};
