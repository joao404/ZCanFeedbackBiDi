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

#include "FeedbackDecoder.h"

FeedbackDecoder::FeedbackDecoder(std::string namespaceFeedbackModul, std::string keyModulConfig, std::array<uint8_t, 8> &trackPin, bool hasRailcom, int configRailcomPin, int configIdPin, bool debug, bool zcanDebug)
    : ZCanInterfaceObserver(zcanDebug),
      m_debug(debug),
      m_hasRailcom(hasRailcom),
      m_configRailcomPin(configRailcomPin),
      m_configIdPin(configIdPin),
      m_modulId(0x0),
      m_idPrgStartTimeINms(0),
      m_idPrgRunning(false),
      m_lastCanCmdSendINms(0),
      m_pingJitterINms(random(0, 100)),
      m_pingIntervalINms(9990 - m_pingJitterINms),
      m_idPrgIntervalINms(60000), // 1 min
      m_masterId(0x0),
      m_sessionId(0x0),
      m_firmwareVersion(0x05010014), // 5.1.20
      m_buildDate(0x07E60917),       // 23.09.2022
      m_hardwareVersion(0x05010001), // 5.1.1
      m_namespaceFeedbackModul{namespaceFeedbackModul},
      m_keyModulConfig{keyModulConfig}
{
    auto sizeTrackData = m_trackData.size();
    auto sizeTrackPin = trackPin.size();
    for (uint8_t i = 0; (i < sizeTrackData) && (i < sizeTrackPin); ++i)
    {
        m_trackData[i].pin = trackPin[i];
        memset(m_trackData[i].adress.begin(), 0, sizeof(TrackData::adress));
    }
}

FeedbackDecoder::~FeedbackDecoder()
{
}

void FeedbackDecoder::begin()
{
    if (!m_preferences.begin(m_namespaceFeedbackModul.c_str(), false))
    {
        Serial.println(F("Access preferences failed"));
    }
    else
    {
        size_t sizeModulConfig{m_preferences.getBytesLength(m_keyModulConfig.c_str())};
        size_t readSize{0};
        if (sizeof(m_modulConfig) == sizeModulConfig)
        {
            readSize = m_preferences.getBytes(m_keyModulConfig.c_str(), static_cast<void *>(&m_modulConfig), sizeModulConfig);
            if (readSize != sizeModulConfig)
            {
                Serial.println(F(" Failed to read modulConfig"));
            }
        }
        else
        {
            Serial.println(F("Wrong size of modulConfig"));
            Serial.println(F("Write default config"));

            m_modulConfig.networkId = modulNidMin + random(1, (modulNidMax - modulNidMin));
            m_modulConfig.modulAdress = 0x00;
            // for (auto finding = m_modulConfig.trackConfig.begin(); finding != m_modulConfig.trackConfig.end(); ++finding)
            m_modulConfig.trackConfig.trackSetCurrentINmA = 10;
            m_modulConfig.trackConfig.trackFreeToSetTimeINms = 0;
            m_modulConfig.trackConfig.trackSetToFreeTimeINms = 1000;
            m_modulConfig.sendChannel2Data = false;

            if (m_preferences.putBytes(m_keyModulConfig.c_str(), &m_modulConfig, sizeof(m_modulConfig)) != sizeof(m_modulConfig))
            {
                Serial.println(F(" Failed to write default modulConfig"));
            }
        }
        m_preferences.end();
    }
    m_networkId = m_modulConfig.networkId;
    m_modulId = m_modulConfig.modulAdress;
    uint32_t day = BUILDTM_DAY;
    uint32_t month = BUILDTM_MONTH;
    uint32_t year = BUILDTM_YEAR;
    m_buildDate = (year << 16) | (month << 8) | day;
    Serial.printf("SW Version: 0x%08X, build date: 0x%08X\n", m_firmwareVersion, m_buildDate);
    Serial.printf("%s: NetworkId %x MA %x CH2 %x\n", m_namespaceFeedbackModul.c_str(), m_networkId, m_modulId, m_modulConfig.sendChannel2Data);

    ZCanInterfaceObserver::begin();

    if (m_hasRailcom)
    {

        pinMode(m_configRailcomPin, INPUT_PULLUP);

        if (!digitalRead(m_configRailcomPin))
        {
            // read current values of adcs as default value
        }

        m_trackData[0].adress[0] = 0x8022;
        m_trackData[2].adress[0] = 0x0023;
        m_trackData[4].adress[0] = 0xC024;
        m_trackData[7].adress[0] = 0x8025;

        m_trackData[0].state = true;
        m_trackData[2].state = true;
        m_trackData[4].state = true;
        m_trackData[6].state = true;
        m_trackData[7].state = true;
    }
    else
    {
        for (uint8_t port = 0; port < m_trackData.size(); ++port)
        {
            pinMode(m_trackData[port].pin, INPUT_PULLUP);
            m_trackData[port].state = digitalRead(m_trackData[port].pin);
            notifyBlockOccupied(port, 0x01, m_trackData[port].state);
            m_trackData[port].lastChangeTimeINms = millis();
        }
    }

    pinMode(m_configIdPin, INPUT_PULLUP);

    // Wait random time before starting logging to Z21
    delay(random(1, 20));

    // Send first ping
    sendPing(m_masterId, m_modulType, m_sessionId);

    Serial.printf("%s finished config\n", m_namespaceFeedbackModul.c_str());
}

void FeedbackDecoder::cyclic()
{
    unsigned long currentTimeINms{millis()};
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        sendPing(m_masterId, m_modulType, m_sessionId);
    }
    if (m_idPrgRunning)
    {
        if ((m_idPrgStartTimeINms + m_idPrgIntervalINms) < currentTimeINms)
        {
            m_idPrgRunning = false;
        }
    }
    if (!digitalRead(m_configIdPin))
    {
        // button pressed
        m_idPrgRunning = true;
        m_idPrgStartTimeINms = currentTimeINms;
    }

    for (uint8_t port = 0; port < m_trackData.size(); ++port)
    {
        bool state = digitalRead(m_trackData[port].pin);
        if (state != m_trackData[port].state)
        {
            if (state)
            {
                if ((m_trackData[port].lastChangeTimeINms + m_modulConfig.trackConfig.trackFreeToSetTimeINms) < currentTimeINms)
                {
                    notifyBlockOccupied(port, 0x01, state);
                }
            }
            else
            {
                if ((m_trackData[port].lastChangeTimeINms + m_modulConfig.trackConfig.trackSetToFreeTimeINms) < currentTimeINms)
                {
                    notifyBlockOccupied(port, 0x01, state);
                }
            }
            m_trackData[port].lastChangeTimeINms = currentTimeINms;
        }
    }
}

void FeedbackDecoder::callbackAccAddrReceived(uint16_t addr)
{
    if (m_idPrgRunning)
    {
        m_modulId = addr;
        m_modulConfig.modulAdress = m_modulId;
        m_idPrgRunning = false;
        saveConfig();
    }
}

void FeedbackDecoder::callbackLocoAddrReceived(uint16_t addr)
{
    // start detection of Railcom
    if (m_hasRailcom)
    {
    }
}

bool FeedbackDecoder::notifyLocoInBlock(uint8_t port, uint16_t adress1, uint16_t adress2, uint16_t adress3, uint16_t adress4)
{
    bool result = sendAccessoryDataEvt(m_modulId, port, 0x11, adress1, adress2);
    result &= sendAccessoryDataEvt(m_modulId, port, 0x12, adress3, adress4);
    return result;
}

bool FeedbackDecoder::notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied)
{
    uint16_t value = occupied ? 0x1100 : 0x0100;
    return sendAccessoryPort6Evt(m_modulId, port, type, value);
}

void FeedbackDecoder::onIdenticalNetworkId()
{
    // received own network id. Generate new random network id
    m_modulConfig.networkId = modulNidMin + random(1, (modulNidMax - modulNidMin));
    saveConfig();
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
                Serial.println("onAccessoryData");
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].adress[0], m_trackData[port].adress[1]);
            }
            else if (0x12 == type)
            {
                Serial.println("onAccessoryData");
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].adress[2], m_trackData[port].adress[3]);
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
                Serial.println("onAccessoryPort6");
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
        Serial.println("onRequestModulInfo");
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
        Serial.println("onModulPowerInfoEvt");
    }
    return true;
}

bool FeedbackDecoder::onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        Serial.println("onModulPowerInfoAck");
    }
    return true;
}

bool FeedbackDecoder::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{
    bool result{false};
    if (id == m_networkId)
    {
        Serial.println("onCmdModulInfo");
        result = true;
        switch (type)
        {
        case 20:
            m_modulConfig.modulAdress = (((info >> 8) & 0xFF) << 8) | (info & 0xFF);
            if (saveConfig())
            {
                m_modulId = m_modulConfig.modulAdress;
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
        Serial.println("onRequestModulObjectConfig");
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
            m_modulConfig.sendChannel2Data = ((value & 0x0010) == 0x0010);
            Serial.printf("Write Send Channel 2 %u\n", m_modulConfig.sendChannel2Data);
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
            m_modulConfig.trackConfig.trackSetCurrentINmA = value;
            Serial.printf("Write SetCurrent %u\n", m_modulConfig.trackConfig.trackSetCurrentINmA);
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
            m_modulConfig.trackConfig.trackFreeToSetTimeINms = value;
            Serial.printf("Write FreeToSetTime %u\n", m_modulConfig.trackConfig.trackFreeToSetTimeINms);
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
            m_modulConfig.trackConfig.trackSetToFreeTimeINms = value;
            Serial.printf("Write SetToFreeTime %u\n", m_modulConfig.trackConfig.trackSetToFreeTimeINms);
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            // all other values are handled
            Serial.printf("Handle tag %x\n", tag);
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
            Serial.printf("New master %x", masterUid);
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

bool FeedbackDecoder::saveConfig()
{
    bool result{false};
    if (!m_preferences.begin(m_namespaceFeedbackModul.c_str(), false))
    {
        Serial.println(F("Access preferences failed"));
    }
    else
    {
        if (m_preferences.putBytes(m_keyModulConfig.c_str(), &m_modulConfig, sizeof(m_modulConfig)) != sizeof(m_modulConfig))
        {
            Serial.println(F(" Failed to write default modulConfig"));
        }
        else
        {
            result = true;
        }
    }
    m_preferences.end();
    return result;
}