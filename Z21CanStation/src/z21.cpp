/*********************************************************************
 * z21
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

#include "z21.h"

z21::z21(uint16_t hash, uint32_t serialNumber, HwType hwType, uint32_t swVersion, void (*printFunc)(const char *, ...), bool debugz21, bool debugZ21, bool debugZCan)
    : ZCanInterfaceObserver(printFunc, debugZCan),
      z21InterfaceObserver(hwType, swVersion, debugZ21),
      m_serialNumber(serialNumber),
      m_debug(debugz21)
{
}

z21::~z21()
{
}

void z21::begin()
{

  ZCanInterfaceObserver::begin();
  z21InterfaceObserver::begin();

  delay(1000);
}

void z21::cyclic()
{
  uint32_t currentTimeINms = millis();
  // TODO send cyclic ping and power voltage information
  if((m_lastPingSendTimeINms + 1000) < currentTimeINms)
  {
    sendPing(0x102FC230, z21Type, 0);
    m_lastPingSendTimeINms = currentTimeINms;
  }
}

void z21::update(Observable &observable, void *data)
{
  ZCanInterfaceObserver::update(observable, data);
  z21InterfaceObserver::update(observable, data);
}

uint16_t z21::getSerialNumber()
{
  return m_serialNumber;
}

// onCallback

void z21::onIdenticalNetworkId()
{
  m_networkId = 0xC000 + random(1, 0xFF);
  sendPing(0x102FC230, z21Type, 0);
}

bool z21::onRequestPing(uint16_t id)
{
  return true;
}

bool z21::onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
  return true;
}

bool z21::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value)
{
  Serial.printf("onAccessoryData %x %x %x %x\n", accessoryId, port, type, value);
  uint8_t data[16];
  data[0] = accessoryId & 0xFF;
  data[1] = accessoryId >> 8;
  data[2] = accessoryId & 0xFF;
  data[3] = accessoryId >> 8;
  data[4] = port;
  data[5] = type;
  data[6] = static_cast<uint8_t>(value & 0xFF);
  data[7] = static_cast<uint8_t>((value >> 8) & 0xFF);
  data[8] = static_cast<uint8_t>((value >> 16) & 0xFF);
  data[9] = static_cast<uint8_t>((value >> 24) & 0xFF);
  EthSend(0x00, 0x0E, z21Interface::Header::LAN_CAN_DETECTOR, data, false, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
  return true;
}

bool z21::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type, uint16_t value)
{
  Serial.printf("onAccessoryPort6 %x %x %x %x\n", accessoryId, port, type, value);
  uint8_t data[16];
  data[0] = accessoryId & 0xFF;
  data[1] = accessoryId >> 8;
  data[2] = accessoryId & 0xFF;
  data[3] = accessoryId >> 8;
  data[4] = port;
  data[5] = 0x1;
  data[6] = static_cast<uint8_t>(value & 0xFF);
  data[7] = static_cast<uint8_t>(value >> 8);
  data[8] = 0;
  data[9] = 0;
  EthSend(0x00, 0x0E, z21Interface::Header::LAN_CAN_DETECTOR, data, false, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
  return true;
}

//--------------------------------------------------------------------------------------------
void z21::notifyz21InterfaceCANdetector(uint8_t client, uint8_t typ, uint16_t ID)
{
  if (0 == typ)
  {
    requestAccessoryData(ID, 0, 0x11);
    requestAccessoryData(ID, 1, 0x11);
    requestAccessoryData(ID, 2, 0x11);
    requestAccessoryData(ID, 3, 0x11);
    requestAccessoryData(ID, 4, 0x11);
    requestAccessoryData(ID, 5, 0x11);
    requestAccessoryData(ID, 6, 0x11);
    requestAccessoryData(ID, 7, 0x11);
    requestAccessoryPort6(ID, 0, 0x01);
    requestAccessoryPort6(ID, 1, 0x01);
    requestAccessoryPort6(ID, 2, 0x01);
    requestAccessoryPort6(ID, 3, 0x01);
    requestAccessoryPort6(ID, 4, 0x01);
    requestAccessoryPort6(ID, 5, 0x01);
    requestAccessoryPort6(ID, 6, 0x01);
    requestAccessoryPort6(ID, 7, 0x01);
  }
}

// void z21::notifyz21InterfaceRailPower(EnergyState State)
// {
//   Serial.print("Power: ");
//   Serial.println(static_cast<uint8_t>(State), HEX);

//   if (EnergyState::csNormal == State)
//   {
//     uint8_t data[16];
//     data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
//     data[1] = 0x01;
//     EthSend(0x00, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
//   }
//   else if (EnergyState::csEmergencyStop == State)
//   {
//     uint8_t data[16];
//     data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
//     data[1] = 0x00; // Power OFF
//     EthSend(0, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
//   }
//   else if (EnergyState::csTrackVoltageOff == State)
//   {
//     uint8_t data[16];
//     data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
//     data[1] = 0x00; // Power OFF
//     EthSend(0, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
//   }
//   z21InterfaceObserver::setPower(State);
// }

// void z21::notifyz21InterfaceLocoState(uint16_t Adr, uint8_t data[])
// {
// }

// void z21::notifyz21InterfaceLocoFkt(uint16_t Adr, uint8_t type, uint8_t fkt)
// {
// }

// //--------------------------------------------------------------------------------------------
// void z21::notifyz21InterfaceLocoSpeed(uint16_t Adr, uint8_t speed, uint8_t stepConfig)
// {
// }

// //--------------------------------------------------------------------------------------------
// void z21::notifyz21InterfaceAccessory(uint16_t Adr, bool state, bool active)
// {
// }

// //--------------------------------------------------------------------------------------------
// void z21::notifyz21InterfaceAccessoryInfo(uint16_t Adr, uint8_t &position)
// {
//   position = 0;
// }

// //--------------------------------------------------------------------------------------------
// void z21::notifyz21InterfacegetSystemInfo(uint8_t client)
// {
//   sendSystemInfo(client, 0, 50000, 77); // report System State to z21Interface clients
// }