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
    : ZCanInterfaceObserver(debug),
      id(nidMin + 5),
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

    pinMode(dccPin, OUTPUT);
    Dcc.pin(dccPin, 0);
    Dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);

    // Wait random time before starting logging to Z21
    delay(random(1, 20));

    // Send first ping
    // TODO
}

void FeedbackDecoder::cyclic()
{

    Dcc.process();
    unsigned long currentTimeINms{millis()};
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        // sendPing();
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

bool FeedbackDecoder::onAccessoryStatus(uint16_t accessoryId)
{
    if (accessoryId == id)
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
    // TODO
    return false;
}

bool FeedbackDecoder::onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
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
    return false;
}

bool FeedbackDecoder::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{
    return false;
}

bool FeedbackDecoder::onRequestPing(uint16_t id)
{
    return false;
}

bool FeedbackDecoder::onPing(uint16_t id, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    return false;
}

bool FeedbackDecoder::sendMessage(ZCanMessage &message)
{
    m_lastCanCmdSendINms = millis();
    return ZCanInterfaceObserver::sendMessage(message);
}