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
#include <Arduino.h>
#include <NmraDcc.h>
#include "ZCan/ZCanInterfaceObserver.h"
#include "Preferences.h"

class FeedbackDecoder : public ZCanInterfaceObserver
{
public:
    FeedbackDecoder(bool debug);
    virtual ~FeedbackDecoder();

    void begin();

    void cyclic();

protected:
    NmraDcc Dcc;
    DCC_MSG Packet;

    uint8_t dccPin{2};

    const uint16_t nidMax{0xD000};
    const uint16_t nidMin{0xDFFF};

    uint16_t id;

    virtual bool onAccessoryStatus(uint16_t accessoryId) override;

    virtual bool onAccessoryMode(uint16_t accessoryId) override;

    virtual bool onAccessoryGpio(uint16_t accessoryId, uint16_t type) override;

    virtual bool onAccessoryPort4(uint16_t accessoryId, uint8_t port) override;

    virtual bool onAccessoryPort4(uint16_t accessoryId, uint8_t port, uint8_t value) override;

    virtual bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type) override;

    virtual bool onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value) override;

    virtual bool onRequestModulPowerInfo(uint16_t id, uint8_t port) override;

    virtual bool onRequestModulInfo(uint16_t id, uint16_t type) override;

    virtual bool onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info) override;

    virtual bool onRequestPing(uint16_t id) override;

    virtual bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) override;

    unsigned long m_lastCanCmdSendINms;

    uint16_t m_pingJitterINms;

    unsigned long m_pingIntervalINms;

    bool sendMessage(ZCanMessage &message) override;

    uint32_t m_masterId;

    uint16_t m_sessionId;

private:
    typedef struct
    {
        uint32_t serialnumber;
        uint32_t builddate;
        uint8_t modulAdress;
        bool sendChannel2Data;
    } ModulConfig;

    ModulConfig m_modulConfig;

    Preferences m_preferences;

    const char *m_namespaceFeedbackModul{"feedbackModul"};

    const char *m_keyModulConfig{"modulConfig"};
};