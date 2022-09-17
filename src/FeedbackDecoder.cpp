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

FeedbackDecoder::FeedbackDecoder(bool debug)
    : ZCanInterfaceObserver(nidMin + 5, debug),
      m_accessoryId(0x0004),
      m_lastCanCmdSendINms(0),
      m_pingJitterINms(random(0, 100)),
      m_pingIntervalINms(9990 - m_pingJitterINms),
      m_masterId(0x0),
      m_sessionId(0x0)
{
    m_modulConfig.serialnumber = 0xFFFF;
    m_modulConfig.builddate = 0xFFFF;
    m_modulConfig.modulAdress = 1;
    m_modulConfig.sendChannel2Data = false;
}

FeedbackDecoder::~FeedbackDecoder()
{
}

void FeedbackDecoder::begin()
{
    if (!m_preferences.begin(m_namespaceFeedbackModul, true))
    {
        Serial.println(F("Access preferences failed"));
    }
    else
    {
        size_t sizeModulConfig{m_preferences.getBytesLength(m_keyModulConfig)};
        size_t readSize{0};
        if (sizeof(m_modulConfig) == sizeModulConfig)
        {
            readSize = m_preferences.getBytes(m_keyModulConfig, static_cast<void *>(&m_modulConfig), sizeModulConfig);
            if (readSize != sizeModulConfig)
            {
                Serial.println(F(" Failed to read modulConfig"));
            }
        }
        else
        {
            Serial.println(F("Wrong size of modulConfig"));
        }
        m_preferences.end();
    }
    Serial.printf("Modul: SN%u BD%u MA%u CH2%u\n",
                  m_modulConfig.serialnumber, m_modulConfig.builddate, m_modulConfig.modulAdress, m_modulConfig.sendChannel2Data);

    pinMode(m_dccPin, OUTPUT);
    m_dcc.pin(m_dccPin, 0);
    m_dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);

    // Wait random time before starting logging to Z21
    delay(random(1, 20));

    // Send first ping
    sendPing(m_masterId, m_type, m_sessionId);
}

void FeedbackDecoder::cyclic()
{

    m_dcc.process();
    unsigned long currentTimeINms{millis()};
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        sendPing(m_masterId, m_type, m_sessionId);
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
    bool result = sendAccessoryDataEvt(m_accessoryId, port, 0x11, adress1, adress2);
    result &= sendAccessoryDataEvt(m_accessoryId, port, 0x12, adress3, adress4);
    return result;
}

bool FeedbackDecoder::notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied)
{
    uint16_t value = occupied ? 0x1100 : 0x0100;
    return sendAccessoryPort6Evt(m_accessoryId, port, type, value);
}

bool FeedbackDecoder::onAccessoryStatus(uint16_t accessoryId)
{
    if (accessoryId == m_accessoryId)
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

bool FeedbackDecoder::onAccessoryMode(uint16_t accessoryId)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessoryGpio(uint16_t accessoryId, uint16_t type)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessoryPort4(uint16_t accessoryId, uint8_t port)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessoryPort4(uint16_t accessoryId, uint8_t port, uint8_t value)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if (accessoryId == m_accessoryId)
    {
        result = sendAccessoryDataAck(m_accessoryId, port, type, 0x8022, 0);
    }
    return result;
}

bool FeedbackDecoder::onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    bool result{false};
    if (accessoryId == m_accessoryId)
    {
        if ((0x11 != type) || (0x12 != type))
        {
            // result = sendAccessoryDataAck(m_accessoryId, port, type, 0x8022, 0);
        }
    }
    return result;
}

bool FeedbackDecoder::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onRequestModulPowerInfo(uint16_t id, uint8_t port)
{
    // TODO
    return false;
}

bool FeedbackDecoder::onRequestModulInfo(uint16_t id, uint16_t type)
{
    // TODO
    //__TIME__
    //__DATE__
    bool result{false};
    if (id == m_networkId)
    {
        result = true;
        switch (type)
        {
        case 1:
            sendModuleInfoAck(type, 0x01020304);
            break;
        case 2:
            sendModuleInfoAck(type, 0x01020304);
            break;
        case 3:
            sendModuleInfoAck(type, 0x01020304);
            break;
        case 4:
            sendModuleInfoAck(type, 0x00010200);
            break;
        case 20:
            sendModuleInfoAck(type, 0x00000005);
            break;
        case 100:
            sendModuleInfoAck(type, m_type);
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
    return false;
}

bool FeedbackDecoder::onRequestPing(uint16_t id)
{
    bool result{false};
    if(id == nidMin)
    {
        result = sendPing(m_masterId, m_type, m_sessionId);
    }
    return result;
}

bool FeedbackDecoder::onPing(uint16_t id, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    if((masterUid != m_masterId) && (0x0200 == (type & 0xFF00)))
    {
        m_masterId = masterUid;
        m_sessionId = sessionId;
    }
    return true;
}

bool FeedbackDecoder::sendMessage(ZCanMessage &message)
{
    m_lastCanCmdSendINms = millis();
    return ZCanInterfaceObserver::sendMessage(message);
}