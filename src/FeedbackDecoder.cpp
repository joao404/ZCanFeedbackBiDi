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

#include "FeedbackDecoder.h"

FeedbackDecoder::FeedbackDecoder(std::string namespaceFeedbackModul, std::string keyModulConfig, bool debug)
    : ZCanInterfaceObserver(debug),
      m_modulId(0x0),
      m_lastCanCmdSendINms(0),
      m_pingJitterINms(random(0, 100)),
      m_pingIntervalINms(9990 - m_pingJitterINms),
      m_masterId(0x0),
      m_sessionId(0x0),
      m_namespaceFeedbackModul{namespaceFeedbackModul},
      m_keyModulConfig{keyModulConfig}
{
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

            m_modulConfig.networkId = nidMin + random(1, 0xFFF);
            m_modulConfig.modulAdress = 0x00;
            m_modulConfig.firmwareVersion = 0x03010014; // 3.1.20
            m_modulConfig.buildDate = 0x07E60923;       // 23.09.2022
            m_modulConfig.hardwareVersion = 0x03010001; // 3.1.1
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
    Serial.printf("Modul: NetworkId %x MA %x CH2 %x\n",
                  m_networkId, m_modulId, m_modulConfig.sendChannel2Data);

    ZCanInterfaceObserver::begin();

    pinMode(m_dccPin, OUTPUT);
    m_dcc.pin(m_dccPin, 0);
    m_dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);

    trackAdress[0] = 0x8022;
    trackAdress[2] = 0x8023;
    trackAdress[4] = 0x8024;
    trackAdress[7] = 0x8025;

    trackState[0] = true;
    trackState[2] = true;
    trackState[4] = true;
    trackState[7] = true;

    // Wait random time before starting logging to Z21
    delay(random(1, 20));

    // Send first ping
    sendPing(m_masterId, m_modulType, m_sessionId);
}

void FeedbackDecoder::cyclic()
{

    m_dcc.process();
    unsigned long currentTimeINms{millis()};
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        sendPing(m_masterId, m_modulType, m_sessionId);
    }
}

void notifyDccMsg(DCC_MSG *Msg)
{
    Serial.print("notifyDccMsg: ");
    for (uint8_t i = 0; i < Msg->Size; i++)
    {
        Serial.print(Msg->Data[i], HEX);
        Serial.write(' ');
    }
    Serial.println();
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
    m_modulConfig.networkId = nidMin + random(1, 0xFFF);
    saveConfig();
    sendPing(m_masterId, m_modulType, m_sessionId);
}

bool FeedbackDecoder::onAccessoryStatus(uint16_t accessoryId)
{//TODO
    if (accessoryId == m_modulId)
    {
        // current CtrlId needed and last time of command
        sendAccessoryStatus(accessoryId, static_cast<uint16_t>(AccessoryErrorCode::NoError), 0, 0xFFFF);
    }
    /*
    void messageAccessoryMode(ZCanMessage &message, uint16_t accessoryId, mx9Present{1});

    void messageAccessoryGpio(ZCanMessage &message, uint16_t accessoryId, 0, Belegtstatus von Gleisen 1-16 uint16_t (oberen 16 bit der 32bit));
  */
    return false;
}

bool FeedbackDecoder::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if (accessoryId == m_modulId)
    {
        result = sendAccessoryDataAck(m_modulId, port, type, 0x8022, 0);
    }
    return result;
}

bool FeedbackDecoder::onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == nidMin))
    {
        if ((0x11 == type) || (0x12 == type))
        {
            if (port < trackState.size())
            {
                result = sendAccessoryDataAck(m_modulId, port, type, trackAdress[port], 0);
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == nidMin))
    {
        if (0x1 == type)
        {
            if (port < trackState.size())
            {
                result = sendAccessoryDataAck(m_modulId, port, type, trackState[port] ? 0x1100 : 0x0100, 0);
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestModulInfo(uint16_t id, uint16_t type)
{
    //__TIME__
    //__DATE__
    bool result{false};
    if (id == m_networkId)
    {
        result = true;
        switch (type)
        {
        case 1: // HwVersion
            sendModuleInfoAck(m_modulId, type, m_modulConfig.hardwareVersion);
            break;
        case 2: // SwVersion
            sendModuleInfoAck(m_modulId, type, m_modulConfig.firmwareVersion);
            break;
        case 3: // Build Date
            sendModuleInfoAck(m_modulId, type, m_modulConfig.buildDate);
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

bool FeedbackDecoder::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{

    bool result{false};
    if (id == m_networkId)
    {
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
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
            m_modulConfig.trackConfig.trackSetCurrentINmA = value;
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
            m_modulConfig.trackConfig.trackFreeToSetTimeINms = value;
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
            m_modulConfig.trackConfig.trackSetToFreeTimeINms = value;
            saveConfig();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestPing(uint16_t id)
{
    bool result{false};
    if (id == nidMin) // TODO
    {
        result = sendPing(m_masterId, m_modulType, m_sessionId);
    }
    return result;
}

bool FeedbackDecoder::onPing(uint16_t id, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    if ((masterUid != m_masterId) && (0x2000 == (type & 0xFF00)))
    {
        Serial.printf("New master %x", masterUid);
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