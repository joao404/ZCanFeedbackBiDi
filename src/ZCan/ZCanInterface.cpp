/*********************************************************************
 * ZCan
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

#include "ZCan/ZCanInterface.h"

size_t printHex(Print &p, unsigned long hex, int digits);
int parseHex(String &s, int start, int end, bool *ok);

ZCanInterface::ZCanInterface(bool debug)
    : m_debug(debug)
{
    if (m_debug)
    {
        Serial.println(F("Creating ZCan"));
    }
}

ZCanInterface::~ZCanInterface()
{
    if (m_debug)
    {
        Serial.println(F("Destroying ZCan"));
    }
}

void ZCanInterface::begin()
{
}

void ZCanInterface::handleReceivedMessage(ZCanMessage &message)
{
    // Serial.print("==> ");
    // Serial.println(message);

    // TODO Handling of NID
    // Wahrscheinlich MX9 ID + DCC accessory adresse

    bool messageHandled{false};
    switch (static_cast<ZCanInterface::Group>(message.group))
    {
    case ZCanInterface::Group::System:
        messageHandled = true;
        break;
    case ZCanInterface::Group::Accessory:
        messageHandled = handleAccessoryMessage(message);
        break;
    case ZCanInterface::Group::Info:
        messageHandled = handleInfoMessage(message);
        break;
    case ZCanInterface::Group::Network:
        messageHandled = handleNetworkMessage(message);
        break;

    default:
        Serial.print("Unsupported message");
        Serial.println(message);
        break;
    }
    if (!messageHandled)
    {
        Serial.print("Unsupported message");
        Serial.println(message);
    }
}

bool ZCanInterface::handleAccessoryMessage(ZCanMessage &message)
{
    bool messageHandled{false};
    uint16_t accessoryId = (message.data[1] << 8) | message.data[0];
    switch (static_cast<AccessoryCmd>(message.command))
    {
    case AccessoryCmd::Status:
        if ((2 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            messageHandled = onAccessoryStatus(accessoryId);
        }
        break;
    case AccessoryCmd::Mode:
        if ((2 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            messageHandled = onAccessoryMode(accessoryId);
        }
        break;
    case AccessoryCmd::Gpio:
        if ((4 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            uint16_t type = (message.data[3] << 8) | message.data[2];
            messageHandled = onAccessoryGpio(accessoryId, type);
        }
        break;
    case AccessoryCmd::Port:
        if (Mode::Req == static_cast<Mode>(message.mode))
        {
            if (3 == message.length)
            {
                uint8_t port = message.data[2];
                messageHandled = onAccessoryPort4(accessoryId, port);
            }
        }
        if (Mode::Cmd == static_cast<Mode>(message.mode))
        {
            if (4 == message.length)
            {
                uint8_t port = message.data[2];
                uint8_t value = message.data[3];
                messageHandled = onAccessoryPort4(accessoryId, port, value);
            }
        }
        break;
    case AccessoryCmd::Data:
        if (Mode::Req == static_cast<Mode>(message.mode))
        {
            if (4 == message.length)
            {
                uint8_t port = message.data[2];
                uint8_t type = message.data[3];
                messageHandled = onAccessoryData(accessoryId, port, type);
            }
        }
        if (Mode::Cmd == static_cast<Mode>(message.mode))
        {
            if (8 == message.length)
            {
                uint8_t port = message.data[2];
                uint8_t type = message.data[3];
                uint32_t value = (message.data[7] << 24) | (message.data[6] << 16) | (message.data[5] << 8) | message.data[4];
                messageHandled = onAccessorySetData(accessoryId, port, type, value);
            }
        }
        break;
        //TODO leftover ons
    default:
        Serial.print("Unkown accessory message:");
        Serial.println(message);
        break;
    }
    return messageHandled;
}

bool ZCanInterface::handleInfoMessage(ZCanMessage &message)
{
    bool messageHandled{false};
    switch (static_cast<InfoCmd>(message.command))
    {
    case InfoCmd::ModulPowerInfo:
        if ((3 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            messageHandled = onRequestModulPowerInfo(id, message.data[2]);
        }
        break;
    case InfoCmd::ModulInfo:
        if ((4 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            uint16_t type = (message.data[3] << 8) | message.data[2];
            messageHandled = onRequestModulInfo(id, type);
        }
        else if ((8 == message.length) && (Mode::Cmd == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            uint16_t type = (message.data[3] << 8) | message.data[2];
            uint32_t info = (message.data[7] << 24) | (message.data[6] << 16) | (message.data[5] << 8) | message.data[4];
            messageHandled = onCmdModulInfo(id, type, info);
        }
        break;
    default:
        Serial.print("Unkown info message:");
        Serial.println(message);
        break;
    }
    return messageHandled;
}

bool ZCanInterface::handleNetworkMessage(ZCanMessage &message)
{
    bool messageHandled{false};
    switch (static_cast<NetworkCmd>(message.command))
    {
    case NetworkCmd::Ping:
        if ((2 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            messageHandled = onRequestPing(id);
        }
        else if ((8 == message.length) && (Mode::Evt == static_cast<Mode>(message.mode)))
        {
            uint32_t masterId = (message.data[3] << 24) | (message.data[2] << 16) | (message.data[1] << 8) | message.data[0];
            uint16_t type = (message.data[5] << 8) | message.data[4];
            uint16_t session = (message.data[7] << 8) | message.data[6];
            messageHandled = onPing(message.networkId, masterId, type, session);
        }
break;
    default:
        Serial.print("Unkown info message:");
        Serial.println(message);
        break;
    }
    return messageHandled;
}

bool ZCanInterface::sendAccessoryStatus(uint16_t accessoryId, uint16_t state, uint16_t ctrlNID, uint16_t lastControlCmdINms)
{
    ZCanMessage message;
    messageAccessoryStatus(message, accessoryId, state, ctrlNID, lastControlCmdINms);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryMode(uint16_t accessoryId, uint16_t mode)
{
    ZCanMessage message;
    messageAccessoryMode(message, accessoryId, mode);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryGpio(uint16_t accessoryId, uint16_t type, uint32_t state)
{
    ZCanMessage message;
    messageAccessoryGpio(message, accessoryId, type, state);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryPort4Evt(uint16_t accessoryId, uint8_t port, uint8_t value)
{
    ZCanMessage message;
    messageAccessoryPort4Evt(message, accessoryId, port, value);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryPort4Ack(uint16_t accessoryId, uint8_t port, bool valid, uint8_t value)
{
    ZCanMessage message;
    messageAccessoryPort4Ack(message, accessoryId, port, valid, value);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryDataEvt(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    ZCanMessage message;
    messageAccessoryDataEvt(message, accessoryId, port, type, value);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryDataAck(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    ZCanMessage message;
    messageAccessoryDataAck(message, accessoryId, port, type, value);
    return sendMessage(message);
}

    bool ZCanInterface::requestModulePowerInfo(uint16_t sendId, uint16_t requestId)
    {
    ZCanMessage message;
    messageRequestModulePowerInfo(message, sendId, requestId);
    return sendMessage(message);
}

    bool ZCanInterface::requestModuleInfo(uint16_t sendId, uint16_t nid, uint16_t type)
    {
    ZCanMessage message;
    messageRequestModuleInfo(message, sendId, nid, type);
    return sendMessage(message);
}

    bool ZCanInterface::getModuleInfo(uint16_t sendId, uint16_t nid, uint16_t type, uint32_t info)
    {
    ZCanMessage message;
    messageCmdModuleInfo(message, sendId, nid, type, info);
    return sendMessage(message);
}

    bool ZCanInterface::sendModuleInfoEvt(uint16_t sendId, uint16_t type, uint32_t info)
    {
    ZCanMessage message;
    messageModuleInfoEvt(message, sendId, type, info);
    return sendMessage(message);
}

bool ZCanInterface::requestPing(uint16_t sendId, uint16_t requestId)
{
    ZCanMessage message;
    messageRequestPing(message, sendId, requestId);
    return sendMessage(message);
}

bool ZCanInterface::sendPing(uint16_t sendId, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    ZCanMessage message;
    messagePing(message, sendId, masterUid, type, sessionId);
    return sendMessage(message);
}

bool ZCanInterface::requestPortOpen(uint16_t id)
{
    ZCanMessage message;
    messageRequestPortOpen(message, id);
    return sendMessage(message);
}

/*private*********************************/

void ZCanInterface::messageAccessoryStatus(ZCanMessage &message, uint16_t accessoryId, uint16_t state, uint16_t CtrlNID, uint16_t lastControlCmdINms)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Status);
    message.mode = static_cast<uint8_t>(Mode::ACK);
    message.networkId = accessoryId; // TODO
    message.length = 0x08;
    message.data[0] = 0xFF & (accessoryId >> 8);
    message.data[1] = 0xFF & accessoryId;
    message.data[2] = 0xFF & (state >> 8);
    message.data[3] = 0xFF & state;
    message.data[4] = 0xFF & (CtrlNID >> 8);
    message.data[5] = 0xFF & CtrlNID;
    message.data[6] = 0xFF & (lastControlCmdINms >> 8);
    message.data[7] = 0xFF & lastControlCmdINms;
}

void ZCanInterface::messageAccessoryMode(ZCanMessage &message, uint16_t accessoryId, uint16_t mode)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Mode);
    message.mode = static_cast<uint8_t>(Mode::ACK);
    message.networkId = accessoryId; // TODO
    message.length = 0x04;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = 0xFF & mode;
    message.data[3] = 0xFF & (mode >> 8);
}

void ZCanInterface::messageAccessoryGpio(ZCanMessage &message, uint16_t accessoryId, uint16_t type, uint32_t state)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Gpio);
    message.mode = static_cast<uint8_t>(Mode::ACK);
    message.networkId = accessoryId; // TODO
    message.length = 0x08;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
    message.data[4] = 0xFF & state;
    message.data[5] = 0xFF & (state >> 8);
    message.data[6] = 0xFF & (state >> 16);
    message.data[7] = 0xFF & (state >> 24);
}

void ZCanInterface::messageAccessoryPort4Evt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Port);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = accessoryId; // TODO
    message.length = 0x04;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = 0x80 | port;
    message.data[3] = value;
}

void ZCanInterface::messageAccessoryPort4Ack(ZCanMessage &message, uint16_t accessoryId, uint8_t port, bool valid, uint8_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Port);
    message.mode = static_cast<uint8_t>(Mode::ACK);
    message.networkId = accessoryId; // TODO
    message.length = 0x04;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = valid ? 0x80 | port : port;
    message.data[3] = value;
}

void ZCanInterface::messageAccessoryDataEvt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Data);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = accessoryId; // TODO
    message.length = 0x08;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value;
    message.data[5] = 0xFF & (value >> 8);
    message.data[6] = 0xFF & (value >> 16);
    message.data[7] = 0xFF & (value >> 24);
}

void ZCanInterface::messageAccessoryDataAck(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Data);
    message.mode = static_cast<uint8_t>(Mode::ACK);
    message.networkId = accessoryId; // TODO
    message.length = 0x08;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value;
    message.data[5] = 0xFF & (value >> 8);
    message.data[6] = 0xFF & (value >> 16);
    message.data[7] = 0xFF & (value >> 24);
}

void ZCanInterface::messageRequestModulePowerInfo(ZCanMessage &message, uint16_t sendId, uint16_t requestId)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulPowerInfo);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = sendId;
    message.length = 0x04;
    message.data[0] = 0xFF & requestId;
    message.data[1] = 0xFF & (requestId >> 8);
}

void ZCanInterface::messageRequestModuleInfo(ZCanMessage &message, uint16_t sendId, uint16_t nid, uint16_t type)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = sendId;
    message.length = 0x04;
    message.data[0] = 0xFF & nid;
    message.data[1] = 0xFF & (nid >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
}

void ZCanInterface::messageCmdModuleInfo(ZCanMessage &message, uint16_t sendId, uint16_t nid, uint16_t type, uint32_t info)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Cmd);
    message.networkId = sendId;
    message.length = 0x08;
    message.data[0] = 0xFF & nid;
    message.data[1] = 0xFF & (nid >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
    message.data[4] = 0xFF & info;
    message.data[5] = 0xFF & (info >> 8);
    message.data[6] = 0xFF & (info >> 16);
    message.data[7] = 0xFF & (info >> 24);
}

void ZCanInterface::messageModuleInfoEvt(ZCanMessage &message, uint16_t sendId, uint16_t type, uint32_t info)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = sendId;
    message.length = 0x06;
    message.data[0] = 0xFF & type;
    message.data[1] = 0xFF & (type >> 8);
    message.data[2] = 0xFF & info;
    message.data[3] = 0xFF & (info >> 8);
    message.data[4] = 0xFF & (info >> 16);
    message.data[5] = 0xFF & (info >> 24);
}

void ZCanInterface::messageRequestPing(ZCanMessage &message, uint16_t sendId, uint16_t requestId)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::Ping);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = sendId;
    message.length = 0x02;
    message.data[0] = 0xFF & requestId;
    message.data[1] = 0xFF & (requestId >> 8);
}

void ZCanInterface::messagePing(ZCanMessage &message, uint16_t sendId, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::Ping);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = sendId;
    message.length = 0x08;
    message.data[0] = 0xFF & masterUid;
    message.data[1] = 0xFF & (masterUid >> 8);
    message.data[2] = 0xFF & (masterUid >> 16);
    message.data[3] = 0xFF & (masterUid >> 24);
    message.data[4] = 0xFF & type;
    message.data[5] = 0xFF & (type >> 8);
    message.data[6] = 0xFF & sessionId;
    message.data[7] = 0xFF & (sessionId >> 8);
}

void ZCanInterface::messageRequestPortOpen(ZCanMessage &message, uint16_t id)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::PortOpen);
    message.mode = static_cast<uint8_t>(Mode::Cmd);
    message.networkId = id;
    message.length = 0x0;
}

// ===================================================================
// === PrintOut Functions=============================================
// ===================================================================

size_t printHex(Print &p, unsigned long hex, int digits)
{
    size_t size = 0;

    String s = String(hex, HEX);

    for (int i = s.length(); i < digits; i++)
    {
        size += p.print("0");
    }

    size += p.print(s);

    return size;
}

int parseHex(String &s, int start, int end, bool *ok)
{
    int value = 0;

    for (int i = start; i < end; i++)
    {
        char c = s.charAt(i);

        if (c >= '0' && c <= '9')
        {
            value = 16 * value + c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            value = 16 * value + 10 + c - 'a';
        }
        else if (c >= 'A' && c <= 'F')
        {
            value = 16 * value + 10 + c - 'A';
        }
        else
        {
            *ok = false;
            return -1;
        }
    }

    return value;
}

// ===================================================================
// === ZCanMessage ==================================================
// ===================================================================

void ZCanMessage::clear()
{
    group = 0;
    command = 0;
    mode = 0;
    networkId = 0;
    length = 0;
    data.fill(0);
}

size_t ZCanMessage::printTo(Print &p) const
{
    size_t size = 0;

    size += printHex(p, group, 2);
    size += p.print(" ");
    size += printHex(p, command, 2);
    size += p.print(" ");
    size += printHex(p, mode, 1);
    size += p.print(" ");
    size += printHex(p, networkId, 1);
    size += p.print(" ");
    size += printHex(p, length, 1);

    for (int i = 0; i < length; i++)
    {
        size += p.print(" ");
        size += printHex(p, data[i], 2);
    }

    return size;
}

bool ZCanMessage::parseFrom(String &s)
{
    bool result = true;

    clear();

    if (s.length() < 11)
    {
        return false;
    }

    group = parseHex(s, 0, 2, &result);
    // TODO:
    // response = s.charAt(5) != ' ';
    command = parseHex(s, 7, 9, &result);
    length = parseHex(s, 10, 11, &result);

    if (length > 8)
    {
        return false;
    }

    if (s.length() < 11 + 3 * length)
    {
        return false;
    }

    for (int i = 0; i < length; i++)
    {
        data[i] = parseHex(s, 12 + 3 * i, 12 + 3 * i + 2, &result);
    }

    return result;
}
