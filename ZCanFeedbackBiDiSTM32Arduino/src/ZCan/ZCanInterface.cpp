/*********************************************************************
 * ZCanInterface
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

ZCanInterface::ZCanInterface(void (*printFunc)(const char *, ...), bool debug)
    : m_debug(debug)
{
    if (nullptr != printFunc)
    {
        m_printFunc = printFunc;
    }
    if (m_debug)
    {
        m_printFunc("Creating ZCan");
    }
}

ZCanInterface::~ZCanInterface()
{
    if (m_debug)
    {
        m_printFunc("Destroying ZCan");
    }
}

void ZCanInterface::begin()
{
}

void ZCanInterface::handleReceivedMessage(ZCanMessage &message)
{
    // m_printFunc("==> ");
    // m_printFunc(message);

    // TODO Handling of NID
    // Wahrscheinlich MX9 ID + DCC accessory adresse

    // received message with own network id. This should normally not happen
    if (message.networkId == m_networkId)
    {
        if (m_debug)
        {
            m_printFunc("Message with identical network id ");
            m_printFunc("%X to %X\n", message.networkId, m_networkId);
            // m_printFunc("%s\n", message.getString().c_str());
        }
        onIdenticalNetworkId();
    }

    // m_printFunc("Message:");
    // m_printFunc(message);

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
        if (m_debug)
        {
            m_printFunc("Unsupported message ");
            m_printFunc("%s\n", message.getString().c_str());
        }
        break;
    }
    if (!messageHandled)
    {
        if (m_debug)
        {
            m_printFunc("Unsupported message ");
            m_printFunc("%s\n", message.getString().c_str());
        }
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
    case AccessoryCmd::Port4:
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
                uint16_t value = (message.data[7] << 24) | (message.data[6] << 16) | (message.data[5] << 8) | message.data[4];
                messageHandled = onAccessorySetData(accessoryId, port, type, value);
            }
        }
        break;
    case AccessoryCmd::Port6:
        if (Mode::Req == static_cast<Mode>(message.mode))
        {
            if (4 == message.length)
            {
                uint8_t port = message.data[2];
                uint8_t type = message.data[3];
                messageHandled = onAccessoryPort6(accessoryId, port, type);
            }
        }
        if (Mode::Cmd == static_cast<Mode>(message.mode))
        {
            if (6 == message.length)
            {
                uint8_t port = message.data[2];
                uint8_t type = message.data[3];
                uint16_t value = (message.data[5] << 8) | message.data[4];
                messageHandled = onAccessoryPort6(accessoryId, port, type, value);
            }
        }
        break;
        // TODO leftover once
    default:
        if (m_debug)
        {
            m_printFunc("Unkown accessory message:");
            m_printFunc("%s\n", message.getString().c_str());
        }
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
        else if ((8 == message.length) && (Mode::Evt == static_cast<Mode>(message.mode)))
        {
            uint16_t nid = message.networkId;
            uint8_t port = message.data[0];
            uint16_t status = (message.data[3] << 8) | message.data[2];
            uint16_t voltageINmV = (message.data[5] << 8) | message.data[4];
            uint16_t currentINmA = (message.data[7] << 8) | message.data[6];
            messageHandled = onModulPowerInfoEvt(nid, port, status, voltageINmV, currentINmA);
        }
        else if ((8 == message.length) && (Mode::Ack == static_cast<Mode>(message.mode)))
        {
            uint16_t nid = message.networkId;
            uint8_t port = message.data[0];
            uint16_t status = (message.data[3] << 8) | message.data[2];
            uint16_t voltageINmV = (message.data[5] << 8) | message.data[4];
            uint16_t currentINmA = (message.data[7] << 8) | message.data[6];
            messageHandled = onModulPowerInfoAck(nid, port, status, voltageINmV, currentINmA);
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
    case InfoCmd::ModulObjectConfig:
        if ((6 == message.length) && (Mode::Req == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            uint32_t tag = (message.data[5] << 24) | (message.data[4] << 16) | (message.data[3] << 8) | message.data[2];
            messageHandled = onRequestModulObjectConfig(id, tag);
        }
        else if ((8 == message.length) && (Mode::Cmd == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            uint32_t tag = (message.data[5] << 24) | (message.data[4] << 16) | (message.data[3] << 8) | message.data[2];
            uint16_t value = (message.data[7] << 8) | message.data[6];
            messageHandled = onCmdModulObjectConfig(id, tag, value);
        }
        if ((8 == message.length) && (Mode::Ack == static_cast<Mode>(message.mode)))
        {
            uint16_t id = (message.data[1] << 8) | message.data[0];
            uint32_t tag = (message.data[5] << 24) | (message.data[4] << 16) | (message.data[3] << 8) | message.data[2];
            uint16_t value = (message.data[7] << 8) | message.data[6];
            messageHandled = onAckModulObjectConfig(id, tag, value);
        }
        break;
    default:
        if (m_debug)
        {
            m_printFunc("Unkown info message:");
            m_printFunc("%s\n", message.getString().c_str());
        }
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
        if (m_debug)
        {
            m_printFunc("Unkown info message:");
            m_printFunc("%s\n", message.getString().c_str());
        }
        break;
    }
    return messageHandled;
}

bool ZCanInterface::onAccessoryStatus(uint16_t accessoryId)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryStatus");
    }
    return false;
}

bool ZCanInterface::onAccessoryMode(uint16_t accessoryId)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryMode");
    }
    return false;
}

bool ZCanInterface::onAccessoryGpio(uint16_t accessoryId, uint16_t type)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryGpio");
    }
    return false;
}

bool ZCanInterface::onAccessoryPort4(uint16_t accessoryId, uint8_t port)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryPort4Req");
    }
    return false;
}

bool ZCanInterface::onAccessoryPort4(uint16_t accessoryId, uint8_t port, uint8_t value)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryPort4");
    }
    return false;
}

bool ZCanInterface::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryData");
    }
    return false;
}

bool ZCanInterface::onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
    if (m_debug)
    {
        m_printFunc("onAccessorySetData");
    }
    return false;
}

bool ZCanInterface::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryPort6Req");
    }
    return false;
}

bool ZCanInterface::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    if (m_debug)
    {
        m_printFunc("onAccessoryPort6");
    }
    return false;
}

bool ZCanInterface::onRequestModulPowerInfo(uint16_t id, uint8_t port)
{
    if (m_debug)
    {
        m_printFunc("onRequestModulPowerInfo");
    }
    return false;
}

bool ZCanInterface::onModulPowerInfoEvt(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        m_printFunc("onModulPowerInfoEvt");
    }
    return false;
}

bool ZCanInterface::onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        m_printFunc("onModulPowerInfoAck");
    }
    return false;
}

bool ZCanInterface::onRequestModulInfo(uint16_t id, uint16_t type)
{
    if (m_debug)
    {
        m_printFunc("onRequestModulInfo");
    }
    return false;
}

bool ZCanInterface::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{
    if (m_debug)
    {
        m_printFunc("onCmdModulInfo");
    }
    return false;
}

bool ZCanInterface::onRequestModulObjectConfig(uint16_t id, uint32_t tag)
{
    if (m_debug)
    {
        m_printFunc("onRequestModulObjectConfig");
    }
    return false;
}

bool ZCanInterface::onCmdModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value)
{
    if (m_debug)
    {
        m_printFunc("onCmdModulObjectConfig");
    }
    return false;
}

bool ZCanInterface::onAckModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value)
{
    if (m_debug)
    {
        m_printFunc("onAckModulObjectConfig");
    }
    return false;
}

bool ZCanInterface::onRequestPing(uint16_t id)
{
    if (m_debug)
    {
        m_printFunc("onRequestPing");
    }
    return false;
}

bool ZCanInterface::onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    if (m_debug)
    {
        m_printFunc("onPing");
    }
    return false;
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

bool ZCanInterface::sendAccessoryDataEvt(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value1, uint16_t value2)
{
    ZCanMessage message;
    messageAccessoryDataEvt(message, accessoryId, port, type, value1, value2);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryDataAck(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value1, uint16_t value2)
{
    ZCanMessage message;
    messageAccessoryDataAck(message, accessoryId, port, type, value1, value2);
    return sendMessage(message);
}

bool ZCanInterface::requestAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    ZCanMessage message;
    messageRequestAccessoryPort6(message, accessoryId, port, type);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryPort6Evt(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    ZCanMessage message;
    messageAccessoryPort6Evt(message, accessoryId, port, type, value);
    return sendMessage(message);
}

bool ZCanInterface::sendAccessoryPort6Ack(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    ZCanMessage message;
    messageAccessoryPort6Ack(message, accessoryId, port, type, value);
    return sendMessage(message);
}

bool ZCanInterface::requestModulePowerInfo(uint16_t id)
{
    ZCanMessage message;
    messageRequestModulePowerInfo(message, id);
    return sendMessage(message);
}

bool ZCanInterface::requestModuleInfo(uint16_t id, uint16_t type)
{
    ZCanMessage message;
    messageRequestModuleInfo(message, id, type);
    return sendMessage(message);
}

bool ZCanInterface::getModuleInfo(uint16_t id, uint16_t type, uint32_t info)
{
    ZCanMessage message;
    messageCmdModuleInfo(message, id, type, info);
    return sendMessage(message);
}

bool ZCanInterface::sendModuleInfoAck(uint16_t type, uint32_t info)
{
    ZCanMessage message;
    messageModuleInfoAck(message, type, info);
    return sendMessage(message);
}

bool ZCanInterface::sendModuleInfoAck(uint16_t id, uint16_t type, uint32_t info)
{
    ZCanMessage message;
    messageModuleInfoAck(message, id, type, info);
    return sendMessage(message);
}

bool ZCanInterface::sendModuleObjectConfigAck(uint16_t id, uint32_t tag, uint16_t value)
{
    ZCanMessage message;
    messageModuleObjectConfigAck(message, id, tag, value);
    return sendMessage(message);
}

bool ZCanInterface::requestPing(uint16_t id)
{
    ZCanMessage message;
    messageRequestPing(message, id);
    return sendMessage(message);
}

bool ZCanInterface::sendPing(uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    ZCanMessage message;
    messagePing(message, masterUid, type, sessionId);
    return sendMessage(message);
}

bool ZCanInterface::requestPortOpen()
{
    ZCanMessage message;
    messageRequestPortOpen(message);
    return sendMessage(message);
}

/*private*********************************/

void ZCanInterface::messageAccessoryStatus(ZCanMessage &message, uint16_t accessoryId, uint16_t state, uint16_t CtrlNID, uint16_t lastControlCmdINms)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Status);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
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
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
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
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
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
    message.command = static_cast<uint8_t>(AccessoryCmd::Port4);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = m_networkId;
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
    message.command = static_cast<uint8_t>(AccessoryCmd::Port4);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x04;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = valid ? 0x80 | port : port;
    message.data[3] = value;
}

void ZCanInterface::messageAccessoryDataEvt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value1, uint16_t value2)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Data);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = m_networkId;
    message.length = 0x08;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value1;
    message.data[5] = 0xFF & (value1 >> 8);
    message.data[6] = 0xFF & value2;
    message.data[7] = 0xFF & (value2 >> 8);
}

void ZCanInterface::messageAccessoryDataAck(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value1, uint16_t value2)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Data);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x08;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value1;
    message.data[5] = 0xFF & (value1 >> 8);
    message.data[6] = 0xFF & value2;
    message.data[7] = 0xFF & (value2 >> 8);
}

void ZCanInterface::messageRequestAccessoryPort6(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Port6);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = m_networkId;
    message.length = 0x04;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
}

void ZCanInterface::messageAccessoryPort6Evt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Port6);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = m_networkId;
    message.length = 0x06;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value;
    message.data[5] = 0xFF & (value >> 8);
}

void ZCanInterface::messageAccessoryPort6Ack(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Accessory);
    message.command = static_cast<uint8_t>(AccessoryCmd::Port6);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x06;
    message.data[0] = 0xFF & accessoryId;
    message.data[1] = 0xFF & (accessoryId >> 8);
    message.data[2] = port;
    message.data[3] = type;
    message.data[4] = 0xFF & value;
    message.data[5] = 0xFF & (value >> 8);
}

void ZCanInterface::messageRequestModulePowerInfo(ZCanMessage &message, uint16_t id)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulPowerInfo);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = m_networkId;
    message.length = 0x04;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
}

void ZCanInterface::messageRequestModuleInfo(ZCanMessage &message, uint16_t id, uint16_t type)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = m_networkId;
    message.length = 0x04;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
}

void ZCanInterface::messageCmdModuleInfo(ZCanMessage &message, uint16_t id, uint16_t type, uint32_t info)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Cmd);
    message.networkId = m_networkId;
    message.length = 0x08;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
    message.data[4] = 0xFF & info;
    message.data[5] = 0xFF & (info >> 8);
    message.data[6] = 0xFF & (info >> 16);
    message.data[7] = 0xFF & (info >> 24);
}

void ZCanInterface::messageModuleInfoAck(ZCanMessage &message, uint16_t type, uint32_t info)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x06;
    message.data[0] = 0xFF & type;
    message.data[1] = 0xFF & (type >> 8);
    message.data[2] = 0xFF & info;
    message.data[3] = 0xFF & (info >> 8);
    message.data[4] = 0xFF & (info >> 16);
    message.data[5] = 0xFF & (info >> 24);
}

void ZCanInterface::messageModuleInfoAck(ZCanMessage &message, uint16_t id, uint16_t type, uint32_t info)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x08;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
    message.data[2] = 0xFF & type;
    message.data[3] = 0xFF & (type >> 8);
    message.data[4] = 0xFF & info;
    message.data[5] = 0xFF & (info >> 8);
    message.data[6] = 0xFF & (info >> 16);
    message.data[7] = 0xFF & (info >> 24);
}

void ZCanInterface::messageModuleObjectConfigAck(ZCanMessage &message, uint16_t id, uint32_t tag, uint16_t value)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Info);
    message.command = static_cast<uint8_t>(InfoCmd::ModulInfo);
    message.mode = static_cast<uint8_t>(Mode::Ack);
    message.networkId = m_networkId;
    message.length = 0x08;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
    message.data[2] = 0xFF & tag;
    message.data[3] = 0xFF & (tag >> 8);
    message.data[4] = 0xFF & (tag >> 16);
    message.data[5] = 0xFF & (tag >> 24);
    message.data[6] = 0xFF & value;
    message.data[7] = 0xFF & (value >> 8);
}

void ZCanInterface::messageRequestPing(ZCanMessage &message, uint16_t id)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::Ping);
    message.mode = static_cast<uint8_t>(Mode::Req);
    message.networkId = m_networkId;
    message.length = 0x02;
    message.data[0] = 0xFF & id;
    message.data[1] = 0xFF & (id >> 8);
}

void ZCanInterface::messagePing(ZCanMessage &message, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::Ping);
    message.mode = static_cast<uint8_t>(Mode::Evt);
    message.networkId = m_networkId;
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

void ZCanInterface::messageRequestPortOpen(ZCanMessage &message)
{
    message.clear();
    message.group = static_cast<uint8_t>(Group::Network);
    message.command = static_cast<uint8_t>(NetworkCmd::PortOpen);
    message.mode = static_cast<uint8_t>(Mode::Cmd);
    message.networkId = m_networkId;
    message.length = 0x0;
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

std::string ZCanMessage::getString()
{
    std::string out;
    char hexString[32];
    sprintf(hexString, "%lX ", group); // convert number to hex
    out += hexString;
    sprintf(hexString, "%lX ", command); // convert number to hex
    out += hexString;
    sprintf(hexString, "%lX ", mode); // convert number to hex
    out += hexString;
    sprintf(hexString, "%lX ", networkId); // convert number to hex
    out += hexString;
    sprintf(hexString, "%X", length); // convert number to hex
    out += hexString;
    for (uint8_t i = 0; i < length; i++)
    {
        sprintf(hexString, " %X", data[i]); // convert number to hex
        out += hexString;
    }
    return out;
}

// bool ZCanMessage::parseFrom(String &s)
// {
//     bool result = true;

//     clear();

//     if (s.length() < 11)
//     {
//         return false;
//     }

//     group = parseHex(s, 0, 2, &result);
//     // TODO:
//     // response = s.charAt(5) != ' ';
//     command = parseHex(s, 7, 9, &result);
//     length = parseHex(s, 10, 11, &result);

//     if (length > 8)
//     {
//         return false;
//     }

//     if (s.length() < 11 + 3 * length)
//     {
//         return false;
//     }

//     for (int i = 0; i < length; i++)
//     {
//         data[i] = parseHex(s, 12 + 3 * i, 12 + 3 * i + 2, &result);
//     }

//     return result;
// }
