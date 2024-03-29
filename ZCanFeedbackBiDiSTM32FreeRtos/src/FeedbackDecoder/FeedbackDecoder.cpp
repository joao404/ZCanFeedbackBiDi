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

#define BUILDTM_YEAR (        \
    __DATE__[7] == '?' ? 1900 \
                       : (((__DATE__[7] - '0') * 1000) + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + __DATE__[10] - '0'))

#define BUILDTM_MONTH (                                 \
    __DATE__[2] == '?'   ? 1                            \
    : __DATE__[2] == 'n' ? (__DATE__[1] == 'a' ? 1 : 6) \
    : __DATE__[2] == 'b' ? 2                            \
    : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4) \
    : __DATE__[2] == 'y' ? 5                            \
    : __DATE__[2] == 'l' ? 7                            \
    : __DATE__[2] == 'g' ? 8                            \
    : __DATE__[2] == 'p' ? 9                            \
    : __DATE__[2] == 't' ? 10                           \
    : __DATE__[2] == 'v' ? 11                           \
                         : 12)

#define BUILDTM_DAY (      \
    __DATE__[4] == '?' ? 1 \
                       : ((__DATE__[4] == ' ' ? 0 : ((__DATE__[4] - '0') * 10)) + __DATE__[5] - '0'))

#include "FeedbackDecoder/FeedbackDecoder.h"
#include <algorithm>
#include <cstring>

FeedbackDecoder::FeedbackDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin,
                                 int configAnalogOffsetPin, int configIdPin, uint8_t &statusLed, void (*printFunc)(const char *, ...),
                                 bool debug, bool zcanDebug)
    : ZCanInterfaceObserver(printFunc, zcanDebug),
      m_debug((nullptr != m_printFunc) ? debug : 0),
      m_saveDataFkt(saveDataFkt),
      m_configAnalogOffsetPin(configAnalogOffsetPin),
      m_configIdPin(configIdPin),
      m_statusLed(statusLed),
      m_modulConfig(modulConfig)
{
    auto sizeTrackData = m_trackData.size();
    auto sizeTrackPin = trackPin.size();
    for (uint8_t i = 0; (i < sizeTrackData) && (i < sizeTrackPin); ++i)
    {
        m_trackData[i].pin = trackPin[i];
        m_trackData[i].changeReported = true; // first report is already done
    }
}

FeedbackDecoder::~FeedbackDecoder()
{
}

void FeedbackDecoder::begin()
{

    pinMode(m_configIdPin, INPUT_PULLUP);

    if ((0xFFFF == m_modulConfig.networkId) || (0x0 == m_modulConfig.networkId))
    {
        // memory not set before
        // write default values
        uint32_t timeINus = micros();
        if (timeINus > (uint32_t)(modulNidMax - modulNidMin))
        {
            m_modulConfig.networkId = modulNidMin + std::max((uint32_t)1, (timeINus / 8));
        }
        else
        {
            m_modulConfig.networkId = modulNidMin + std::max((uint32_t)1, timeINus);
        }
        m_modulConfig.modulAddress = 0x00;
        // for (auto finding = m_modulConfig.trackConfig.begin(); finding != m_modulConfig.trackConfig.end(); ++finding)
        m_modulConfig.trackConfig.trackSetCurrentINmA = 10;
        m_modulConfig.trackConfig.trackFreeToSetTimeINms = 20;
        m_modulConfig.trackConfig.trackSetToFreeTimeINms = 1000;
        m_modulConfig.sendChannel2Data = 0;
        m_saveDataFkt();
    }

    // m_modulConfig.networkId = 0x9201;
    m_networkId = m_modulConfig.networkId;
    m_modulId = m_modulConfig.modulAddress;
    uint32_t day = BUILDTM_DAY;
    uint32_t month = BUILDTM_MONTH;
    uint32_t year = BUILDTM_YEAR;
    m_buildDate = (year << 16) | (month << 8) | day;
    // 3300mV per 4096 bits is 0.8mVperCount
    // I have a 22 Ohm resistor so 0.8mV per Count * 22 * current is offset down below
    // so I take 8*22 = 17,6 is round about 18
    m_trackSetVoltage = 18 * m_modulConfig.trackConfig.trackSetCurrentINmA;
    ZCanInterfaceObserver::m_printFunc("SW Version: 0x%08X, build date: 0x%08X\n", m_firmwareVersion, m_buildDate);
    ZCanInterfaceObserver::m_printFunc("NetworkId %x MA %x CH2 %x\n", m_networkId, m_modulId, m_modulConfig.sendChannel2Data);
    ZCanInterfaceObserver::m_printFunc("trackSetCurrentINmA: %d\n", m_modulConfig.trackConfig.trackSetCurrentINmA);
    ZCanInterfaceObserver::m_printFunc("trackFreeToSetTimeINms: %d\n", m_modulConfig.trackConfig.trackFreeToSetTimeINms);
    ZCanInterfaceObserver::m_printFunc("trackSetToFreeTimeINms: %d\n", m_modulConfig.trackConfig.trackSetToFreeTimeINms);
    ZCanInterfaceObserver::m_printFunc("trackSetVoltage: %d\n", m_trackSetVoltage);

    m_pingJitterINms = std::max((uint32_t)0, std::min((micros() / 10), (uint32_t)100));
    m_pingIntervalINms = (9990 - m_pingJitterINms);

    ZCanInterfaceObserver::begin();

    configInputs();

    for (uint8_t port = 0; port < m_trackData.size(); ++port)
    {
        ZCanInterfaceObserver::m_printFunc("Offset from memory port %d: %d\n", port, m_trackData[port].voltageOffset);
    }

    // Wait random time before starting logging to Z21
    delay(millis() / 2);
    // Send first ping
    sendPing(m_masterId, m_modulType, m_sessionId);

    ZCanInterfaceObserver::m_printFunc("%X finished config\n", m_networkId);
}

void FeedbackDecoder::configInputs()
{
    for (uint8_t port = 0; port < m_trackData.size(); ++port)
    {
        pinMode(m_trackData[port].pin, INPUT_PULLUP);
        m_trackData[port].state = !digitalRead(m_trackData[port].pin);
        m_trackData[port].reportedState = m_trackData[port].state;
        notifyBlockOccupied(port, 0x01, m_trackData[port].state);
        m_trackData[port].lastChangeTimeINms = millis();
        if (m_debug)
            ZCanInterfaceObserver::m_printFunc("port: %d state:%d\n", port, m_trackData[port].state);
    }
}

void FeedbackDecoder::cyclic()
{
    unsigned long currentTimeINms{millis()};
    ///////////////////////////////////////////////////////////////////////////
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        sendPing(m_masterId, m_modulType, m_sessionId);
        m_lastCanCmdSendINms = currentTimeINms;
    }
    ///////////////////////////////////////////////////////////////////////////
    if (m_idPrgRunning)
    {
        if ((m_idPrgStartTimeINms + m_idPrgIntervalINms) < currentTimeINms)
        {
            m_idPrgRunning = false;
        }
    }
    ///////////////////////////////////////////////////////////////////////////
    if (!digitalRead(m_configIdPin))
    {
        // button pressed
        m_idPrgRunning = true;
        m_idPrgStartTimeINms = currentTimeINms;
    }
    ///////////////////////////////////////////////////////////////////////////
    cyclicPortCheck();
    checkDelayedStatusChange();
}

void FeedbackDecoder::cyclicPortCheck()
{
    bool state = !digitalRead(m_trackData[m_detectionPort].pin);
    checkPortStatusChange(state);
    m_detectionPort++;
    if (m_trackData.size() <= m_detectionPort)
    {
        m_detectionPort = 0;
    }
}

void FeedbackDecoder::checkDelayedStatusChange()
{
    uint8_t index{0};
    for (auto &port : m_trackData)
    {
        if (port.state != port.reportedState)
        {
            if (!port.changeReported)
            {
                uint32_t currentTimeINms = millis();
                if (port.state)
                {
                    if ((port.lastChangeTimeINms + m_modulConfig.trackConfig.trackFreeToSetTimeINms) < currentTimeINms)
                    {
                        port.changeReported = true;
                        port.reportedState = port.state;
                        notifyBlockOccupied(index, 0x01, port.state);
                        onBlockOccupied();
                        if (m_debug)
                            ZCanInterfaceObserver::m_printFunc("p: %d s:%d\n", index, port.state);
                    }
                }
                else
                {
                    if ((port.lastChangeTimeINms + m_modulConfig.trackConfig.trackSetToFreeTimeINms) < currentTimeINms)
                    {
                        port.changeReported = true;
                        port.reportedState = port.state;
                        notifyBlockOccupied(index, 0x01, port.state);
                        onBlockEmpty(index);
                        if (m_debug)
                            ZCanInterfaceObserver::m_printFunc("p: %d s:%d\n", index, port.state);
                    }
                }
            }
        }
        index++;
    }
}

void FeedbackDecoder::callbackDccReceived()
{
}

void FeedbackDecoder::callbackAccAddrReceived(uint16_t addr)
{
    if (m_idPrgRunning)
    {
        m_modulId = addr;
        m_modulConfig.modulAddress = m_modulId;
        m_idPrgRunning = false;
        m_saveDataFkt();
    }
}

void FeedbackDecoder::callbackLocoAddrReceived(uint16_t addr)
{
}

void FeedbackDecoder::callbackAdcReadFinished(ADC_HandleTypeDef *hadc)
{
}

bool FeedbackDecoder::notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied)
{
    uint16_t value = occupied ? 0x1100 : 0x0100;
    bitWrite(m_statusLed, port, occupied);
    return sendAccessoryPort6Evt(m_modulId, port, type, value);
}

void FeedbackDecoder::checkPortStatusChange(bool state)
{
    if (state != m_trackData[m_detectionPort].state)
    {
        m_trackData[m_detectionPort].changeReported = false;
        m_trackData[m_detectionPort].state = state;
        m_trackData[m_detectionPort].lastChangeTimeINms = millis();
    }
}

void FeedbackDecoder::onBlockOccupied()
{
}

void FeedbackDecoder::onBlockEmpty(size_t blockNum)
{
}

//---------------------------------------------------------------------------
// ZCan Callbacks

void FeedbackDecoder::onIdenticalNetworkId()
{
    // received own network id. Generate new random network id
    m_modulConfig.networkId = modulNidMin + std::max((uint16_t)1, (uint16_t)(millis() % (modulNidMax - modulNidMin)));
    m_saveDataFkt();
    sendPing(m_masterId, m_modulType, m_sessionId);
}

bool FeedbackDecoder::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == modulNidMin))
    {
        if (port < m_trackData.size())
        {
            if (0x11 == type)
            {
                if (m_debug)
                    ZCanInterfaceObserver::m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, 0, 0);
            }
            else if (0x12 == type)
            {
                if (m_debug)
                    ZCanInterfaceObserver::m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, 0, 0);
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == modulNidMin))
    {
        if (0x1 == type)
        {
            if (port < m_trackData.size())
            {
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].state ? 0x1100 : 0x0100, 0);
                if (m_debug)
                    ZCanInterfaceObserver::m_printFunc("onAccessoryPort6\n");
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestModulInfo(uint16_t id, uint16_t type)
{
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            ZCanInterfaceObserver::m_printFunc("onRequestModulInfo\n");
        result = true;
        switch (type)
        {
        case 1: // HwVersion
            sendModuleInfoAck(m_modulId, type, m_hardwareVersion);
            break;
        case 2: // SwVersion
            sendModuleInfoAck(m_modulId, type, m_firmwareVersion);
            break;
        case 3: // Build Date
            sendModuleInfoAck(m_modulId, type, m_buildDate);
            break;
        case 4:
            sendModuleInfoAck(m_modulId, type, 0x00010200);
            break;
        case 20: // modul Id
            sendModuleInfoAck(m_modulId, type, m_modulId);
            break;
        case 100:
            sendModuleInfoAck(m_modulId, type, m_modulType);
            break;

        default:
            result = false;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onModulPowerInfoEvt(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        ZCanInterfaceObserver::m_printFunc("onModulPowerInfoEvt\n");
    }
    return true;
}

bool FeedbackDecoder::onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        ZCanInterfaceObserver::m_printFunc("onModulPowerInfoAck\n");
    }
    return true;
}

bool FeedbackDecoder::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            ZCanInterfaceObserver::m_printFunc("onCmdModulInfo\n");
        result = true;
        switch (type)
        {
        case 20:
            m_modulConfig.modulAddress = (((info >> 8) & 0xFF) << 8) | (info & 0xFF);
            if (m_saveDataFkt())
            {
                m_modulId = m_modulConfig.modulAddress;
            }
            sendModuleInfoAck(m_modulId, type, m_modulId);
            break;

        default:
            result = false;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestModulObjectConfig(uint16_t id, uint32_t tag)
{
    // send requested configuration
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            ZCanInterfaceObserver::m_printFunc("onRequestModulObjectConfig %x %x\n", id, tag);
        uint16_t value{0};
        switch (tag)
        {
        case 0x00221001:
        case 0x00221002:
        case 0x00221003:
        case 0x00221004:
        case 0x00221005:
        case 0x00221006:
        case 0x00221007:
        case 0x00221008:
            value = (m_modulConfig.sendChannel2Data ? 0x0010 : 0x0000) | 0x0001;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
        case 0x00401002:
        case 0x00401003:
        case 0x00401004:
        case 0x00401005:
        case 0x00401006:
        case 0x00401007:
        case 0x00401008:
            value = m_modulConfig.trackConfig.trackSetCurrentINmA;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
        case 0x00501002:
        case 0x00501003:
        case 0x00501004:
        case 0x00501005:
        case 0x00501006:
        case 0x00501007:
        case 0x00501008:
            value = m_modulConfig.trackConfig.trackFreeToSetTimeINms;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
        case 0x00511002:
        case 0x00511003:
        case 0x00511004:
        case 0x00511005:
        case 0x00511006:
        case 0x00511007:
        case 0x00511008:
            value = m_modulConfig.trackConfig.trackSetToFreeTimeINms;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onCmdModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value)
{
    // write requested configuration
    // send requested configuration
    bool result{false};
    if (id == m_networkId)
    {
        switch (tag)
        {
        case 0x00221001:
            m_modulConfig.sendChannel2Data = ((value & 0x0010) == 0x0010) ? 1 : 0;
            if (m_debug)
                ZCanInterfaceObserver::m_printFunc("Write Send Channel 2 %u\n", m_modulConfig.sendChannel2Data);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
            m_modulConfig.trackConfig.trackSetCurrentINmA = value;
            m_trackSetVoltage = 18 * value;
            if (m_debug)
            {
                ZCanInterfaceObserver::m_printFunc("Write SetCurrent %u\n", m_modulConfig.trackConfig.trackSetCurrentINmA);
                ZCanInterfaceObserver::m_printFunc("Write track set voltage %u\n", m_trackSetVoltage);
            }
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
            m_modulConfig.trackConfig.trackFreeToSetTimeINms = value;
            if (m_debug)
                ZCanInterfaceObserver::m_printFunc("Write FreeToSetTime %u\n", m_modulConfig.trackConfig.trackFreeToSetTimeINms);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
            m_modulConfig.trackConfig.trackSetToFreeTimeINms = value;
            if (m_debug)
                ZCanInterfaceObserver::m_printFunc("Write SetToFreeTime %u\n", m_modulConfig.trackConfig.trackSetToFreeTimeINms);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            // all other values are handled
            ZCanInterfaceObserver::m_printFunc("Handle tag %x\n", tag);
            result = true;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestPing(uint16_t id)
{
    bool result{false};
    if ((id == m_modulId) || ((id & 0xF000) == modulNidMin))
    {
        result = sendPing(m_masterId, m_modulType, m_sessionId);
    }
    return result;
}

bool FeedbackDecoder::onPing(uint16_t id, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    if ((masterUid != m_masterId) && (0x2000 == (type & 0xFF00)))
    {
        if (m_debug)
        {
            ZCanInterfaceObserver::m_printFunc("New master %x\n", masterUid);
        }
        m_masterId = masterUid;
        m_sessionId = sessionId;
        sendPing(m_masterId, m_modulType, m_sessionId);
    }
    return true;
}

bool FeedbackDecoder::sendMessage(ZCanMessage &message)
{
    m_lastCanCmdSendINms = millis();
    return ZCanInterfaceObserver::sendMessage(message);
}