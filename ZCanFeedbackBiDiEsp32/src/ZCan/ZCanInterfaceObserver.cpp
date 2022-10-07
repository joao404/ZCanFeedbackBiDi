/*********************************************************************
 * ZCanInterfaceObserver
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

#include "ZCan/ZCanInterfaceObserver.h"

ZCanInterfaceObserver::ZCanInterfaceObserver(bool debug, void (*printFunc)(const char *, ...))
    : ZCanInterface(debug, printFunc)
{
}

ZCanInterfaceObserver::~ZCanInterfaceObserver()
{
  end();
}

bool ZCanInterfaceObserver::setCanObserver(std::shared_ptr<CanInterface> canInterface)
{
  m_canInterface = canInterface;
  return nullptr != m_canInterface;
}

void ZCanInterfaceObserver::begin()
{
  m_canInterface->attach(*this);

  ZCanInterface::begin();
}

void ZCanInterfaceObserver::end()
{
}

void ZCanInterfaceObserver::update(Observable &observable, void *data)
{
  if (&observable == m_canInterface.get())
  {
    if (nullptr != data)
    {
      CanInterface::CanMessage *frame = static_cast<CanInterface::CanMessage *>(data);

      ZCanMessage message;
      message.clear();
      message.group = (frame->identifier >> 24) & 0x0f;
      message.command = (frame->identifier >> 18) & 0x3f;
      message.mode = (frame->identifier >> 16) & 0x03;
      message.networkId = ((frame->identifier & (1<<16)) ==  (1 << 16));
      message.length = frame->data_length_code;
      message.data = frame->data;

#ifdef CAN_DEBUG
      if (m_debug)
      {
        Serial.print("==> ");
        Serial.print(frame->identifier, HEX);
        Serial.print(" ");
        Serial.println(message);
      }
#endif
      handleReceivedMessage(message);
    }
  }
}

bool ZCanInterfaceObserver::sendMessage(ZCanMessage &message)
{
  CanInterface::CanMessage txFrame;

  // message.hash = m_hash;

  txFrame.identifier = (1 << 28) | (static_cast<uint32_t>(message.group) << 24) | (static_cast<uint32_t>(message.command) << 18) | (static_cast<uint32_t>(message.mode) << 16) | static_cast<uint32_t>(message.networkId);
  txFrame.extd = 1;
  txFrame.ss = 1;
  txFrame.data_length_code = message.length;
  txFrame.data = message.data;

#ifdef CAN_DEBUG
  if (m_debug)
  {
    Serial.print("<== ");
    Serial.print(txFrame.identifier, HEX);
    Serial.print(" ");
    Serial.println(message);
  }
#endif
  bool result{false};
  if (nullptr != m_canInterface.get())
  {
    result = m_canInterface->transmit(txFrame, 100u);
  }
  return result;
}

bool ZCanInterfaceObserver::receiveMessage(ZCanMessage &message)
{
  CanInterface::CanMessage rxFrame;

  bool result{false};

  if (nullptr != m_canInterface.get())
  {
    result = m_canInterface->receive(rxFrame, 200u);
  }

  if (result)
  {
    message.clear();
    message.group = (rxFrame.identifier >> 24) & 0x0f;
    message.command = (rxFrame.identifier >> 18) & 0x3f;
    message.mode = (rxFrame.identifier >> 16) & 0x03;
    message.networkId = ((rxFrame.identifier & (1<<16)) ==  (1 << 16));
    message.length = rxFrame.data_length_code;
    message.data = rxFrame.data;

#ifdef CAN_DEBUG
    if (m_debug)
    {
      Serial.print("==> ");
      Serial.println(message);
    }
#endif
  }

  return result;
}