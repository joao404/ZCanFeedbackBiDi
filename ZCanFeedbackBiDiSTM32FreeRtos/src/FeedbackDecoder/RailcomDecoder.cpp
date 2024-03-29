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

#include "FeedbackDecoder/RailcomDecoder.h"

RailcomDecoder::RailcomDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                               int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...),
                               bool debug, bool zcanDebug, bool railcomDebug)
    : FeedbackDecoder(modulConfig, saveDataFkt, trackPin, configAnalogOffsetPin, configIdPin, statusLed, printFunc, debug, zcanDebug),
      m_railcomDebug((nullptr != m_printFunc) ? railcomDebug : 0)
{
}
RailcomDecoder::~RailcomDecoder()
{
}

void RailcomDecoder::configInputs()
{
    pinMode(m_configAnalogOffsetPin, INPUT_PULLUP);
    //  this is done outside of FeedbackDecoder
    if (!digitalRead(m_configAnalogOffsetPin))
    {
        ZCanInterfaceObserver::m_printFunc("Offset measuring\n");
        configAdcSingleMode();
        // read current values of adcs as default value
        for (uint8_t port = 0; port < m_trackData.size(); ++port)
        {
            m_trackData[port].voltageOffset = singleAdcRead(m_trackData[port].pin);
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
    // notify initial state which is no occupied
    // for (uint8_t port = 0; port < m_trackData.size(); ++port)
    // {
    //     m_trackData[port].state = false;
    //     notifyBlockOccupied(port, 0x01, m_trackData[port].state);
    //     m_trackData[port].lastChangeTimeINms = millis();
    //     if (m_debug)
    //         ZCanInterfaceObserver::m_printFunc("port: %d state:%d\n", port, m_trackData[port].state);
    // }
    // for (uint8_t port = 0; port < m_railcomData.size(); ++port)
    // {
    //     for (auto &data : m_railcomData[port].railcomAddr)
    //     {
    //         data.address = 0;
    //         data.changeReported = true;
    //         data.direction = 0;

    //     }
    //     notifyLocoInBlock(port, m_railcomData[port].railcomAddr);
    // }
    m_currentSenseDataReady = xSemaphoreCreateBinary();
    if (nullptr == m_currentSenseDataReady)
    {
        Serial.println(F("Semaphore m_currentSenseDataReady failed"));
        while (1)
            ;
    }
    m_railcomSenseDataReady = xSemaphoreCreateBinary();
    if (nullptr == m_railcomSenseDataReady)
    {
        Serial.println(F("Semaphore m_railcomSenseDataReady failed"));
        while (1)
            ;
    }
    m_railcomDetectionPort = 0;
    m_railcomDetectionMeasurement = 0;
    configAdcDmaMode();
}

void RailcomDecoder::cyclicPortCheck()
{
    if (pdTRUE == xSemaphoreTake(m_currentSenseDataReady, 0))
    {
        uint32_t m_currentSenseSum{0};
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
            triggerDmaRead(m_trackData[m_detectionPort].pin, (uint32_t *)m_adcDmaBufferCurrentSense.begin(), m_adcDmaBufferCurrentSense.size());
        }
        else
        {
            // no more measurements
            m_currentSenseRunning = false;
        }
    }
    ///////////////////////////////////////////////////////////////////////////
    // process Railcom data from ADC
    if (pdTRUE == xSemaphoreTake(m_railcomSenseDataReady, 0))
    {
        // std::array<uint16_t, 512> dmaBuffer;
        //  copy DMA buffer to make sure that data is not overwritten in case that we take to long to analyze
        // dmaBuffer = m_adcDmaBuffer;
        analyzeRailcomData((uint16_t *)m_adcDmaBufferRailcom.begin(), 400, m_trackData[m_railcomDetectionPort].voltageOffset, m_trackSetVoltage);
        m_addrReceived = AddressType::eNone;
        // prepare already next measurement
        m_railcomDetectionMeasurement++;
        if (m_maxNumberOfConsecutiveMeasurements <= m_railcomDetectionMeasurement)
        {
            m_railcomDetectionMeasurement = 0;
            if (m_trackData.size() <= (m_railcomDetectionPort + 1))
            {
                m_railcomDetectionPort = 0;
            }
            else
            {
                m_railcomDetectionPort++;
            }
        }

        //  trigger measurement of current sense
        m_detectionPort = 0;
        m_currentSenseRunning = true;
        m_railcomSenseRunning = false;
        triggerDmaRead(m_trackData[m_detectionPort].pin, (uint32_t *)m_adcDmaBufferCurrentSense.begin(), m_adcDmaBufferCurrentSense.size()); // 26 us
    }

    // check for address data which was not renewed
    for (auto &data : m_railcomData[m_cyclicRailcomCheckPort].railcomAddr)
    {
        // check if a address, that was already reported, was not refreshed since timeout
        if (0 != data.address)
        {
            if ((m_railcomDataTimeoutINms + data.lastChangeTimeINms) < millis())
            {
                if (m_railcomDebug)
                {
                    m_printFunc("L leftTime:0x%X %u\n", data.address, m_cyclicRailcomCheckPort);
                }
                data.address = 0;
                data.direction = 0;
                data.lastChangeTimeINms = millis();
                // TODO: save portnumber with trackData to use more for functions
                // notifyLocoInBlock(m_railcomDetectionPort, m_trackData[m_detectionPort].railcomAddr);
                notifyLocoInBlock(m_cyclicRailcomCheckPort, m_railcomData[m_cyclicRailcomCheckPort].railcomAddr);
            }
        }
    }
    m_cyclicRailcomCheckPort++;
    if (m_cyclicRailcomCheckPort >= m_railcomData.size())
    {
        m_cyclicRailcomCheckPort = 0;
    }
}

void RailcomDecoder::onBlockOccupied()
{
    notifyLocoInBlock(m_detectionPort, m_railcomData[m_detectionPort].railcomAddr);
}

void RailcomDecoder::onBlockEmpty(size_t blockNum)
{
    // for (auto &block : m_railcomData)
    // {
    //     for (auto &loco : block.railcomAddr)
    //     {
    //         ZCanInterfaceObserver::m_printFunc("Blocks:0x%X %d\n", loco.address, loco.changeReported);
    //     }
    // }
    RailcomData &block{m_railcomData[blockNum]};
    for (RailcomAddr &railcomAddr : block.railcomAddr)
    {
        if (0 != railcomAddr.address)
        {
            if (m_railcomDebug)
            {
                ZCanInterfaceObserver::m_printFunc("L leftBlock:0x%X %u\n", railcomAddr.address, blockNum);
            }
        }
        railcomAddr.address = 0;
        railcomAddr.direction = 0;
        railcomAddr.lastChangeTimeINms = millis();
        railcomAddr.changeReported = false;
    }
    m_railcomData[blockNum].channel1Data[0].id = 0;
    m_railcomData[blockNum].channel1Data[1].id = 0;
    m_railcomData[blockNum].channel1Data[0].data[0] = 0;
    m_railcomData[blockNum].channel1Data[1].data[0] = 0;
    notifyLocoInBlock(blockNum, m_railcomData[blockNum].railcomAddr);
}

void RailcomDecoder::callbackDccReceived()
{
    if (!m_currentSenseRunning && !m_railcomSenseRunning)
    {
        m_railcomSenseRunning = true;
        // after DMA was executed, configure next channel already to save time
        triggerDmaRead(m_trackData[m_railcomDetectionPort].pin, (uint32_t *)m_adcDmaBufferRailcom.begin(), m_adcDmaBufferRailcom.size()); // 26 us
    }
}

void RailcomDecoder::callbackAccAddrReceived(uint16_t addr)
{
    m_lastRailcomAddress = addr;
    m_addrReceived = AddressType::eAcc;
    FeedbackDecoder::callbackAccAddrReceived(addr);
}

void RailcomDecoder::callbackLocoAddrReceived(uint16_t addr)
{
    m_lastRailcomAddress = addr;
    m_addrReceived = AddressType::eLoco;
}

void RailcomDecoder::callbackAdcReadFinished(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken{pdFALSE};
    if (m_railcomSenseRunning)
    {
        xSemaphoreGiveFromISR(m_railcomSenseDataReady, &xHigherPriorityTaskWoken);
    }
    else if (m_currentSenseRunning)
    {
        xSemaphoreGiveFromISR(m_currentSenseDataReady, &xHigherPriorityTaskWoken);
    }
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

bool RailcomDecoder::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == modulNidMin))
    {
        if (port < m_trackData.size())
        {
            if (0x11 == type)
            {
                if (m_railcomDebug)
                    ZCanInterfaceObserver::m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, (m_railcomData[port].railcomAddr[0].direction << 14) | m_railcomData[port].railcomAddr[0].address, (m_railcomData[port].railcomAddr[1].direction << 14) | m_railcomData[port].railcomAddr[1].address);
            }
            else if (0x12 == type)
            {
                if (m_railcomDebug)
                    ZCanInterfaceObserver::m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, (m_railcomData[port].railcomAddr[2].direction << 14) | m_railcomData[port].railcomAddr[2].address, (m_railcomData[port].railcomAddr[3].direction << 14) | m_railcomData[port].railcomAddr[3].address);
            }
        }
    }
    return result;
}

// analyze incoming bit stream for railcom data and act accordingly
void RailcomDecoder::analyzeRailcomData(uint16_t dmaBufferIN1samplePer1us[], size_t length, uint16_t voltageOffset, uint16_t trackSetVoltage)
{
    RailcomChannelData channel1;
    RailcomChannelData channel2;
    m_channel1Direction = 0;
    m_channel2Direction = 0;
    m_dmaBufferIN1samplePer1us = dmaBufferIN1samplePer1us;
    // get possible uart bytes of serial communication including start position in stream and polarity to check direction
    handleBitStream(dmaBufferIN1samplePer1us, length, channel1, channel2, voltageOffset, trackSetVoltage);

    // bool dataReceivedChannel1{false};

    if (channel1.size >= 2)
    {
        // at least 2 bytes
        // run through analytics of every two bytes
        size_t maxIndex{channel1.size - 1};
        for (size_t i = 0; i < maxIndex; i++)
        {

            uint8_t highByte{channel1.bytes[i].data};
            uint8_t lowByte{channel1.bytes[i + 1].data};
            if ((highByte < 0x40) && (lowByte < 0x40))
            {
                if ((channel1.bytes[i + 1].startIndex - channel1.bytes[i].endIndex) < 6) // one byte commes direct after another
                {
                    // check if start index of first byte is near second byte start index
                    uint8_t railcomValue = ((highByte & 0x03) << 6) | (lowByte & 0x3F);
                    RailcomPacket12Bit packet({highByte, lowByte});
                    if ((1 == packet.id) || (2 == packet.id))
                    {
                        uint16_t locoAddr{0};
                        if (0x01 == packet.id)
                        {
                            if (m_railcomData[m_railcomDetectionPort].channel1Data[0].id != packet.id)
                            {
                                m_railcomData[m_railcomDetectionPort].channel1Data[0].id = 1;
                                m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] = railcomValue;
                            }
                            else if (m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] != railcomValue)
                            {
                                m_railcomData[m_railcomDetectionPort].channel1Data[1].id = 0;
                                m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0] = 0;
                            }
                        }
                        else if (0x02 == packet.id)
                        {
                            if (m_railcomData[m_railcomDetectionPort].channel1Data[1].id != packet.id)
                            {
                                m_railcomData[m_railcomDetectionPort].channel1Data[1].id = 2;
                                m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0] = railcomValue;
                            }
                            else if (m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0] != railcomValue)
                            {
                                m_railcomData[m_railcomDetectionPort].channel1Data[0].id = 0;
                                m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] = 0;
                            }
                        }
                        if ((1 == m_railcomData[m_railcomDetectionPort].channel1Data[0].id) && (2 == m_railcomData[m_railcomDetectionPort].channel1Data[1].id))
                        {
                            if (0x00 == (m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] & 0xB0))
                            {
                                // Base address CV1
                                locoAddr = m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0] & 0x7F;
                            }
                            else if (0x60 == (m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] & 0xFF))
                            {
                                // Multiple Traction address CV19
                                locoAddr = m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0] & 0x7F;
                            }
                            else if (0x80 == (m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] & 0xB0))
                            {
                                // Extended address CV17 + CV18
                                locoAddr = ((m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0] & 0x3F) << 8) | m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0];
                            }
                        }

                        if ((4 == channel1.bytes[i].direction) && (4 == channel1.bytes[i + 1].direction))
                        {
                            m_channel1Direction = 0x10;
                        }
                        else if ((-4 == channel1.bytes[i].direction) && (-4 == channel1.bytes[i + 1].direction))
                        {
                            m_channel1Direction = 0x11;
                        }

                        std::array<uint16_t, 4> data = {m_railcomData[m_railcomDetectionPort].channel1Data[0].id, m_railcomData[m_railcomDetectionPort].channel1Data[0].data[0], m_railcomData[m_railcomDetectionPort].channel1Data[1].id, m_railcomData[m_railcomDetectionPort].channel1Data[1].data[0]};
                        handleFoundLocoAddr(locoAddr, m_channel1Direction, Channel::eChannel1, data);

                        // m_printFunc("L %X %X %X %X\n", railcomId, railcomValue, m_railcomData[m_railcomDetectionPort].lastChannelId, m_railcomData[m_railcomDetectionPort].lastChannelData);

                        // m_railcomData[m_railcomDetectionPort].lastChannelId = railcomId;
                        // m_railcomData[m_railcomDetectionPort].lastChannelData = railcomValue;
                        // dataReceivedChannel1 = true;
                        break;
                    }
                }
            }
        }
    }
    // if (!dataReceivedChannel1)
    // {
    //     m_railcomData[m_railcomDetectionPort].lastChannelId = 0xFF;
    //     m_railcomData[m_railcomDetectionPort].lastChannelData = 0xFF;
    // }

    // channel 2
    if (channel2.size > 0)
    {
        if (AddressType::eLoco == m_addrReceived)
        {
            // find first NACK/ACK/BUSY => end of transmission
            size_t endOfMessage{channel2.size};
            for (size_t i = 0; i < channel2.size; i++)
            {
                uint8_t lastByte = channel2.bytes[i].data;
                if ((0x40 == lastByte) || (0x41 == lastByte) || (0x42 == lastByte))
                {
                    // NACK, ACK, BUSY
                    if (4 == channel2.bytes[i].direction)
                    {
                        m_channel2Direction = 0x10;
                    }
                    if (-4 == channel2.bytes[i].direction)
                    {
                        m_channel2Direction = 0x11;
                    }
                    std::array<uint16_t, 4> data = {1, 2, 3, 4};
                    handleFoundLocoAddr(m_lastRailcomAddress, m_channel2Direction, Channel::eChannel2, data);
                    endOfMessage = i;
                    // no relevant data afterwards
                    break;
                }
            }
            // if (endOfMessage != channel2.size)
            // {
            //     // there could be some data
            //     switch (endOfMessage)
            //     {
            //     case 0:
            //     case 1:
            //     case 2:
            //     case 3:
            //     case 4:
            //     case 5:
            //     case 6:
            //     {
            //         size_t i=0;
            //         uint8_t railcomId{static_cast<uint8_t>((channel2.bytes[i].data >> 2u) & 0xFu)};
            //         switch (railcomId)
            //         {
            //         case 0:
            //         case 3:
            //         case 7:
            //         case 12:

            //             if ((channel2.bytes[i + 1].startIndex - channel2.bytes[i].endIndex) < 6) // one byte commes direct after another
            //             {
            //                 uint8_t highByte{channel2.bytes[i].data};
            //                 uint8_t lowByte{channel2.bytes[i + 1].data};
            //                 uint8_t railcomId = (highByte >> 2) & 0xF;

            //                 if (4 == channel2.bytes[i].direction)
            //                 {
            //                     m_channel2Direction = 0x10;
            //                 }
            //                 if (-4 == channel2.bytes[i].direction)
            //                 {
            //                     m_channel2Direction = 0x11;
            //                 }
            //                 std::array<uint16_t, 4> data = {1, 2, 3, 4};
            //                 handleFoundLocoAddr(m_lastRailcomAddress, m_channel2Direction, Channel::eChannel2, data);
            //                 break;
            //             }
            //             break;

            //         default:
            //             break;
            //         }
            //         break;
            //     }
            //     default:
            //         break;
            //     }
            // }
        }
        else if (AddressType::eAcc == m_addrReceived)
        {
        }
    }
}

// retrive parameters of next byte in bit stream
bool RailcomDecoder::getStartAndStopByteOfUart(bool *bitStreamIN1samplePer1us, size_t startIndex, size_t endIndex,
                                               size_t *findStartIndex, size_t *findEndIndex)
{
    bool result{false};
    // search for first high level
    while (startIndex < endIndex)
    {
        if (bitStreamIN1samplePer1us[startIndex] && !bitStreamIN1samplePer1us[startIndex + 1])
        {
            // negativ flank found
            *findStartIndex = startIndex;

            *findEndIndex = *findStartIndex + 37;
            if (*findEndIndex <= endIndex)
            {
                result = true;
            }
            break;
        }
        startIndex++;
    }
    return result;
}

void RailcomDecoder::handleBitStream(uint16_t dmaBufferIN1samplePer1us[], size_t length, RailcomChannelData &channel1, RailcomChannelData &channel2, uint16_t voltageOffset, uint16_t trackSetVoltage)
{
    std::array<bool, 512> bitStreamDataBuffer;

    auto iteratorBit = bitStreamDataBuffer.begin();

    size_t maxIterator{length > bitStreamDataBuffer.size() ? bitStreamDataBuffer.size() : length};

    for (size_t i = 0; i < length; i++)
    {
        if (dmaBufferIN1samplePer1us[i] > voltageOffset)
        {
            *iteratorBit = ((dmaBufferIN1samplePer1us[i] - voltageOffset) < trackSetVoltage);
        }
        else
        {
            *iteratorBit = ((voltageOffset - dmaBufferIN1samplePer1us[i]) < trackSetVoltage);
        }
        iteratorBit++;
    }

    bool *bitStreamIN1samplePer1us{bitStreamDataBuffer.begin()};

    auto analyzeStream = [this, &bitStreamIN1samplePer1us, &voltageOffset](RailcomChannelData &channel, size_t startOfSearch, size_t endOfSearch)
    {
        size_t startIndex{0};
        size_t endIndex{0};
        uint8_t numberOfBytes{0};

        while (RailcomDecoder::getStartAndStopByteOfUart(bitStreamIN1samplePer1us, startOfSearch, endOfSearch, &startIndex, &endIndex))
        {
            // found
            uint8_t dataByte{0};
            uint8_t bit{0};
            int8_t directionCount{0};
            channel.bytes[numberOfBytes].startIndex = startIndex;
            channel.bytes[numberOfBytes].endIndex = endIndex;
            startIndex += 6; // add 6 bits to get to middle of first data bit
            size_t endOfByte{startIndex + 28};
            while (endOfByte >= startIndex)
            {
                if (!bitStreamIN1samplePer1us[startIndex])
                {
                    // zero bits means that value is higher or lower than idle value
                    if (m_dmaBufferIN1samplePer1us[startIndex] > voltageOffset)
                    {
                        // there will always be four zeros if transmission is correct
                        // TODO
                        directionCount++;
                    }
                    else
                    {
                        // TODO
                        directionCount--;
                    }
                }
                dataByte |= ((bitStreamIN1samplePer1us[startIndex] ? 1 : 0) << bit++);
                startIndex += 4;
            }
            if (8 == bit)
            {
                // from 4 to 8 code
                dataByte = encode8to4[dataByte];
                switch (dataByte)
                {
                case 0xEE:
                case 0xFF:
                    // not used => error
                    break;
                default:
                    channel.bytes[numberOfBytes].data = dataByte;
                    channel.bytes[numberOfBytes].direction = directionCount;
                    channel.bytes[numberOfBytes].valid = true;
                    numberOfBytes++;
                    break;
                }
            }
            startOfSearch = endIndex;
        }
        channel.size = numberOfBytes;
    };

    // railcom pulse is at least 22us high while measurement was startet and last pulse has roundabout 40 seconds
    const size_t endOfChannel1{150}; // 170us (channel 1) minus 40us
    size_t endOfSearch{(length - 1) > endOfChannel1 ? endOfChannel1 : length - 1};
    analyzeStream(channel1, 15, endOfSearch);

    const size_t startChannel2{150}; // 10u after end of channel 1
    analyzeStream(channel2, startChannel2, length - 1);
}

void RailcomDecoder::handleFoundLocoAddr(uint16_t locoAddr, uint16_t direction, Channel channel, std::array<uint16_t, 4> &railcomData)
{
    if ((0 != locoAddr) && (255 != locoAddr))
    {
        bool addressFound{false};
        for (auto &data : m_railcomData[m_railcomDetectionPort].railcomAddr)
        {
            if (locoAddr == data.address)
            {
                addressFound = true;
                if (direction != data.direction)
                {
                    data.direction = direction;
                    if (m_railcomDebug)
                    {
                        m_printFunc("dir:0x%X 0x%X %d\n", locoAddr, direction, channel);
                    }
                    notifyLocoInBlock(m_railcomDetectionPort, m_railcomData[m_railcomDetectionPort].railcomAddr);
                }
                data.lastChangeTimeINms = millis();
                break;
            }
        }
        if (!addressFound)
        {
            // value not in table, find a block for it. If full, ignore value
            for (auto &data : m_railcomData[m_railcomDetectionPort].railcomAddr)
            {
                if (0 == data.address)
                {
                    data.address = locoAddr;
                    data.direction = direction;
                    if (m_railcomDebug)
                    {
                        m_printFunc("come:0x%X D:0x%X %d:%d\n", locoAddr, direction, m_railcomDetectionPort, channel);
                        // m_printFunc("%x %x %x %x\n", railcomData[0], railcomData[1], railcomData[2], railcomData[3]);
                    }
                    notifyLocoInBlock(m_railcomDetectionPort, m_railcomData[m_railcomDetectionPort].railcomAddr);
                    data.lastChangeTimeINms = millis();
                    break;
                }
            }
        }
        // if (Channel::eChannel1 == channel)
        // {
        //     // switch to next port because already one address for channel 1 found
        // }
    }
}

bool RailcomDecoder::notifyLocoInBlock(uint8_t port, std::array<RailcomAddr, 4> railcomAddr)
{
    bool result = sendAccessoryDataEvt(m_modulId, port, 0x11,
                                       (railcomAddr[0].direction << 14) | railcomAddr[0].address, (railcomAddr[1].direction << 14) | railcomAddr[1].address);
    result &= sendAccessoryDataEvt(m_modulId, port, 0x12,
                                   (railcomAddr[2].direction << 14) | railcomAddr[2].address, (railcomAddr[3].direction << 14) | railcomAddr[3].address);
    return result;
}

uint8_t RailcomDecoder::encode4to8[] = {
    0b10101100,
    0b10101010,
    0b10101001,
    0b10100101,
    0b10100011,
    0b10100110,
    0b10011100,
    0b10011010,
    0b10011001,
    0b10010101,
    0b10010011,
    0b10010110,
    0b10001110,
    0b10001101,
    0b10001011,
    0b10110001,
    0b10110010,
    0b10110100,
    0b10111000,
    0b01110100,
    0b01110010,
    0b01101100,
    0b01110010,
    0b01101100,
    0b01101010,
    0b01101001,
    0b01100101,
    0b01100011,
    0b01100110,
    0b01011100,
    0b01011010,
    0b01011001,
    0b01010101,
    0b01010011,
    0b01010110,
    0b01001110,
    0b01001101,
    0b01001011,
    0b01000111,
    0b01110001,
    0b11101000,
    0b11100100,
    0b11100010,
    0b11010001,
    0b11001001,
    0b11000101,
    0b11011000,
    0b11010100,
    0b11010010,
    0b11001010,
    0b11000110,
    0b11001100,
    0b01111000,
    0b00010111,
    0b00011011,
    0b00011101,
    0b00011110,
    0b00101110,
    0b00110110,
    0b00111010,
    0b00100111,
    0b00101011,
    0b00101101,
    0b00110101,
    0b00111001,
    0b00110011, // 0x3F

    0b00001111, // NACK
    0b11110000, // ACK
    0b11100001, // BUSY
    0b11000011, // not used
    0b10000111, // not used
    0b00111100  // not used
};
// NACK 0x40
// ACK 0x41
// Busy 0x42
uint8_t RailcomDecoder::encode8to4[] = {
    0xFF, // invalid 0b00000000
    0xFF, // invalid 0b00000001
    0xFF, // invalid 0b00000010
    0xFF, // invalid 0b00000011
    0xFF, // invalid 0b00000100
    0xFF, // invalid 0b00000101
    0xFF, // invalid 0b00000110
    0xFF, // invalid 0b00000111
    0xFF, // invalid 0b00001000
    0xFF, // invalid 0b00001001
    0xFF, // invalid 0b00001010
    0xFF, // invalid 0b00001011
    0xFF, // invalid 0b00001100
    0xFF, // invalid 0b00001101
    0xFF, // invalid 0b00001110
    0x40, // NACK 15 0b00001111
    0xFF, // invalid 0b00010000
    0xFF, // invalid 0b00010001
    0xFF, // invalid 0b00010010
    0xFF, // invalid 0b00010011
    0xFF, // invalid 0b00010100
    0xFF, // invalid 0b00010101
    0xFF, // invalid 0b00010110
    0x33, // 0x17    0b00010111
    0xFF, // invalid 0b00011000
    0xFF, // invalid 0b00011001
    0xFF, // invalid 0b00011010
    0x34, // 0x1B    0b00011011
    0xFF, // invalid 0b00011100
    0x35, // 0x1D    0b00011101
    0x36, // 0x1E    0b00011110
    0xFF, // invalid 0b00011111
    0xFF, // invalid 0b00100000
    0xFF, // invalid 0b00100001
    0xFF, // invalid 0b00100010
    0xFF, // invalid 0b00100011
    0xFF, // invalid 0b00100100
    0xFF, // invalid 0b00100101
    0xFF, // invalid 0b00100110
    0x3A, // 0x27    0b00100111
    0xFF, // invalid 0b00101000
    0xFF, // invalid 0b00101001
    0xFF, // invalid 0b00101010
    0x3B, // 0x2B    0b00101011
    0xFF, // invalid 0b00101100
    0x3C, // invalid 0b00101101
    0x37, // 0x2E    0b00101110
    0xFF, // invalid 0b00101111
    0xFF, // invalid 0b00110000
    0xFF, // invalid 0b00110001
    0xFF, // invalid 0b00110010
    0x3F, // 0x33    0b00110011
    0xFF, // invalid 0b00110100
    0x3D, // 0x35    0b00110101
    0x38, // 0x36    0b00110110
    0xFF, // invalid 0b00110111
    0xFF, // invalid 0b00111000
    0x3E, // 0x39    0b00111001
    0x39, // 0x3A    0b00111010
    0xFF, // invalid 0b00111011
    0xEE, // not use 0b00111100
    0xFF, // invalid 0b00111101
    0xFF, // invalid 0b00111110
    0xFF, // invalid 0b00111111
    0xFF, // invalid 0b01000000
    0xFF, // invalid 0b01000001
    0xFF, // invalid 0b01000010
    0xFF, // invalid 0b01000011
    0xFF, // invalid 0b01000100
    0xFF, // invalid 0b01000101
    0xFF, // invalid 0b01000110
    0x24, // 0x47    0b01000111
    0xFF, // invalid 0b01001000
    0xFF, // invalid 0b01001001
    0xFF, // invalid 0b01001010
    0x23, // 0x4B    0b01001011
    0xFF, // invalid 0b01001100
    0x22, // 0x4D    0b01001101
    0x21, // 0x4E    0b01001110
    0xFF, // invalid 0b01001111
    0xFF, // invalid 0b01010000
    0xFF, // invalid 0b01010001
    0xFF, // invalid 0b01010010
    0x1F, // 0x53    0b01010011
    0xFF, // invalid 0b01010100
    0x1E, // 0x55    0b01010101
    0x20, // 0x56    0b01010110
    0xFF, // invalid 0b01010111
    0xFF, // invalid 0b01011000
    0x1D, // 0x59    0b01011001
    0x1C, // 0x5A    0b01011010
    0xFF, // invalid 0b01011011
    0x1B, // 0x5C    0b01011100
    0xFF, // invalid 0b01011101
    0xFF, // invalid 0b01011110
    0xFF, // invalid 0b01011111
    0xFF, // invalid 0b01100000
    0xFF, // invalid 0b01100001
    0xFF, // invalid 0b01100010
    0x19, // 0x63    0b01100011
    0xFF, // invalid 0b01100100
    0x18, // 0x65    0b01100101
    0x1A, // 0x66    0b01100110
    0xFF, // invalid 0b01100111
    0xFF, // invalid 0b01101000
    0x17, // 0x69    0b01101001
    0x16, // 0x6A    0b01101010
    0xFF, // invalid 0b01101011
    0x15, // 0x6C    0b01101100
    0xFF, // invalid 0b01101101
    0xFF, // invalid 0b01101110
    0xFF, // invalid 0b01101111
    0xFF, // invalid 0b01110000
    0x25, // 0x71    0b01110001
    0x14, // 0x72    0b01110010
    0xFF, // invalid 0b01110011
    0x13, // 0x74    0b01110100
    0xFF, // invalid 0b01110101
    0xFF, // invalid 0b01110110
    0xFF, // invalid 0b01110111
    0x32, // 0x78    0b01111000
    0xFF, // invalid 0b01111001
    0xFF, // invalid 0b01111010
    0xFF, // invalid 0b01111011
    0xFF, // invalid 0b01111100
    0xFF, // invalid 0b01111101
    0xFF, // invalid 0b01111110
    0xFF, // invalid 0b01111111
    0xFF, // invalid 0b10000000
    0xFF, // invalid 0b10000001
    0xFF, // invalid 0b10000010
    0xFF, // invalid 0b10000011
    0xFF, // invalid 0b10000100
    0xFF, // invalid 0b10000101
    0xFF, // invalid 0b10000110
    0xEE, // not use 0b10000111
    0xFF, // invalid 0b10001000
    0xFF, // invalid 0b10001001
    0xFF, // invalid 0b10001010
    0x0E, // 0x8B    0b10001011
    0xFF, // invalid 0b10001100
    0x0D, // 0x8D    0b10001101
    0x0C, // 0x8E    0b10001110
    0xFF, // invalid 0b10001111
    0xFF, // invalid 0b10010000
    0xFF, // invalid 0b10010001
    0xFF, // invalid 0b10010010
    0x0A, // 0x93    0b10010011
    0xFF, // invalid 0b10010100
    0x09, // 0x95    0b10010101
    0x0B, // 0x96    0b10010110
    0xFF, // invalid 0b10010111
    0xFF, // invalid 0b10011000
    0x08, // 0x99    0b10011001
    0x07, // 0x9A    0b10011010
    0xFF, // invalid 0b10011011
    0x06, // 0x9C    0b10011100
    0xFF, // invalid 0b10011101
    0xFF, // invalid 0b10011110
    0xFF, // invalid 0b10011111
    0xFF, // invalid 0b10100000
    0xFF, // invalid 0b10100001
    0xFF, // invalid 0b10100010
    0x04, // 0xA3    0b10100011
    0xFF, // invalid 0b10100100
    0x03, // 0xA5    0b10100101
    0x05, // 0xA6    0b10100110
    0xFF, // invalid 0b10100111
    0xFF, // invalid 0b10101000
    0x02, // 0xA9    0b10101001
    0x01, // 0xAA    0b10101010
    0xFF, // invalid 0b10101011
    0x00, // 0xAC    0b10101100
    0xFF, // invalid 0b10101101
    0xFF, // invalid 0b10101110
    0xFF, // invalid 0b10101111
    0xFF, // invalid 0b10110000
    0x0F, // 0xB1    0b10110001
    0x10, // 0xB2    0b10110010
    0xFF, // invalid 0b10110011
    0x11, // 0xB4    0b10110100
    0xFF, // invalid 0b10110101
    0xFF, // invalid 0b10110110
    0xFF, // invalid 0b10110111
    0x12, // 0xB8    0b10111000
    0xFF, // invalid 0b10111001
    0xFF, // invalid 0b10111010
    0xFF, // invalid 0b10111011
    0xFF, // invalid 0b10111100
    0xFF, // invalid 0b10111101
    0xFF, // invalid 0b10111110
    0xFF, // invalid 0b10111111
    0xFF, // invalid 0b11000000
    0xFF, // invalid 0b11000001
    0xFF, // invalid 0b11000010
    0xEE, // not use 0b11000011
    0xFF, // invalid 0b11000100
    0x2B, // 0xC5    0b11000101
    0x30, // 0xC6    0b11000110
    0xFF, // invalid 0b11000111
    0xFF, // invalid 0b11001000
    0x2A, // 0xC9    0b11001001
    0x2F, // 0xCA    0b11001010
    0xFF, // invalid 0b11001011
    0x31, // 0xCC    0b11001100
    0xFF, // invalid 0b11001101
    0xFF, // invalid 0b11001110
    0xFF, // invalid 0b11001111
    0xFF, // invalid 0b11010000
    0x29, // 0xD1    0b11010001
    0x2E, // 0xD2    0b11010010
    0xFF, // invalid 0b11010011
    0x2D, // 0xD4    0b11010100
    0xFF, // invalid 0b11010101
    0xFF, // invalid 0b11010110
    0xFF, // invalid 0b11010111
    0x2C, // 0xD8    0b11011000
    0xFF, // invalid 0b11011001
    0xFF, // invalid 0b11011010
    0xFF, // invalid 0b11011011
    0xFF, // invalid 0b11011100
    0xFF, // invalid 0b11011101
    0xFF, // invalid 0b11011110
    0xFF, // invalid 0b11011111
    0xFF, // invalid 0b11100000
    0x42, // BUSY    0b11100001
    0x28, // 0xE2    0b11100010
    0xFF, // invalid 0b11100011
    0x27, // 0xE4    0b11100100
    0xFF, // invalid 0b11100101
    0xFF, // invalid 0b11100110
    0xFF, // invalid 0b11100111
    0x26, // 0xE8    0b11101000
    0xFF, // invalid 0b11101001
    0xFF, // invalid 0b11101010
    0xFF, // invalid 0b11101011
    0xFF, // invalid 0b11101100
    0xFF, // invalid 0b11101101
    0xFF, // invalid 0b11101110
    0x41, // ACK     0b11110000
    0xFF, // invalid 0b11110001
    0xFF, // invalid 0b11110010
    0xFF, // invalid 0b11110011
    0xFF, // invalid 0b11110100
    0xFF, // invalid 0b11110101
    0xFF, // invalid 0b11110110
    0xFF, // invalid 0b11110111
    0xFF, // invalid 0b11111000
    0xFF, // invalid 0b11111001
    0xFF, // invalid 0b11111010
    0xFF, // invalid 0b11111011
    0xFF, // invalid 0b11111100
    0xFF, // invalid 0b11111101
    0xFF, // invalid 0b11111110
    0xFF, // invalid 0b11111111
};