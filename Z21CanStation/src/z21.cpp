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

z21::z21(ConfigDccStation &configDccStation, uint16_t hash, uint32_t serialNumber, HwType hwType, uint32_t swVersion, void (*printFunc)(const char *, ...), bool debugz21, bool debugZ21, bool debugZCan)
    : ZCanInterfaceObserver(printFunc, debugZCan),
      z21InterfaceObserver(hwType, swVersion, debugZ21),
      m_dccPin(configDccStation.dccPin),
      m_ndccPin(configDccStation.ndccPin),
      m_enablePin1(configDccStation.enablePin1),
      m_enablePin2(configDccStation.enablePin2),
      m_shortPin(configDccStation.shortPin),
      m_shortCircuitThresholdINmA(configDccStation.shortCircuitThresholdINmA),
      m_serialNumber(serialNumber),
      m_debug(debugz21)
{
}

z21::~z21()
{
}

void z21::begin()
{
  if (!m_preferences.begin(m_namespaceZ21, true))
  {
    Serial.println(F("Access preferences failed"));
  }
  else
  {
    ConfigLoco buffer[256];
    size_t sizeLocoMode = m_preferences.getBytesLength(m_keyLocoMode);
    size_t readSize = 0;
    Serial.print(F("sizeLocoMode "));
    Serial.println(sizeLocoMode);
    if (0 != sizeLocoMode)
    {
      readSize = m_preferences.getBytes(m_keyLocoMode, buffer, sizeLocoMode);
      if (readSize != sizeLocoMode)
      {
        Serial.println(F(" Failed to read locoMode"));
      }
    }
    Serial.print(F("readSize "));
    Serial.println(readSize);
    readSize /= sizeof(ConfigLoco);
    for (size_t i = 0; i < readSize; i++)
    {
      m_locos.emplace_back(ConfigLoco{buffer[i].adr, buffer[i].steps});
    }

    // if (preferences.getBytes(keyTurnOutMode, turnOutMode, sizeof(turnOutMode)) != sizeof(turnOutMode))
    // {
    //   Serial.println(F(" Failed to read turnOutMode"));
    // }
    m_preferences.end();
  }

  pinMode(m_enablePin1, OUTPUT);
  pinMode(m_enablePin2, OUTPUT);

  digitalWrite(m_enablePin1, LOW);
  digitalWrite(m_enablePin2, LOW);

  m_dps.setup(m_dccPin, m_ndccPin, DCC128, ROCO, OFF); // with Railcom but shutdown by default
  m_dps.setrailcom();                                  // enable railcom

  Serial.print("Power: ");
  Serial.println(m_dps.getpower());

  ZCanInterfaceObserver::begin();
  z21InterfaceObserver::begin();

  m_xMutex = xSemaphoreCreateMutex(); // crete a mutex object

  delay(1000);
}

void z21::cyclic()
{
  uint32_t currentTimeINms = millis();
  // TODO send cyclic ping and power voltage information
  if ((m_lastPingSendTimeINms + 1000) < currentTimeINms)
  {
    sendPing(0x102FC230, z21Type, 0);
    m_lastPingSendTimeINms = currentTimeINms;
  }
  // m_dps.update();

  // short circuit detection using ACS712 5A
  uint32_t sensorINmV{analogReadMilliVolts(m_shortPin)};
  uint32_t currentINmV{static_cast<uint32_t>((sensorINmV > offsetINmV) ? sensorINmV - offsetINmV : offsetINmV - sensorINmV)};
  m_currentINmA = static_cast<uint16_t>(currentINmV / milliVoltagePerAmpere);

  if (m_currentINmA > m_shortCircuitThresholdINmA)
  { // Short Circuit!
    if ((OFF != m_dps.getpower()) && (SHORT != m_dps.getpower()))
    {
      digitalWrite(m_enablePin1, LOW);
      digitalWrite(m_enablePin2, LOW);
      if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
      {
        m_dps.setpower(SHORT);
        xSemaphoreGive(m_xMutex); // release the mutex
      }
      Serial.printf("Short Circuit:%d %d %d %f\n", sensorINmV, offsetINmV, currentINmV, m_currentINmA);
      uint8_t data[16];
      data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
      data[1] = 0x00; // Power OFF
      EthSend(0, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));

      z21InterfaceObserver::setPower(EnergyState::csTrackVoltageOff);
    }
  }
}

void z21::dcc()
{
  if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
  {
    m_dps.update();
    xSemaphoreGive(m_xMutex); // release the mutex
  }
  else
  {
    Serial.println("dcc");
  }
}

void z21::update(Observable &observable, void *data)
{
  ZCanInterfaceObserver::update(observable, data);
  z21InterfaceObserver::update(observable, data);
}

void z21::saveLocoConfig()
{
  ConfigLoco buffer[256];

  size_t index = 0;
  for (ConfigLoco &n : m_locos)
  {  
    buffer[index] = n;
    index++;
  }

  if (!m_preferences.begin(m_namespaceZ21, false))
  {
    Serial.println(F("Access preferences failed"));
  }
  else
  {
    if (m_preferences.putBytes(m_keyLocoMode, buffer, m_locos.size() * sizeof(ConfigLoco)) != m_locos.size() * sizeof(ConfigLoco))
    {
      Serial.println(F(" Failed to write locoMode"));
    }
    m_preferences.end();
  }
}

void z21::deleteLocoConfig()
{
  if (!m_preferences.begin(m_namespaceZ21, false))
  {
    Serial.println(F("Access preferences failed"));
  }
  else
  {
    if (!m_preferences.remove(m_keyLocoMode))
    {
      Serial.println(F(" Failed to delete locoMode"));
    }
    else
    {
      // delete all locos currently in memory to prevent writing again
      m_locos.clear();
    }
    m_preferences.end();
  }
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

void z21::notifyz21InterfaceRailPower(EnergyState State)
{
  Serial.print("Power: ");
  Serial.println(static_cast<uint8_t>(State), HEX);

  if (EnergyState::csNormal == State)
  {
    digitalWrite(m_enablePin1, HIGH);
    digitalWrite(m_enablePin2, HIGH);
    if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
    {
      m_dps.setpower(ON);       // enable power
      xSemaphoreGive(m_xMutex); // release the mutex
    }
    else
    {
      Serial.println("m_dps.setpower(ON);");
    }
    uint8_t data[16];
    data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
    data[1] = 0x01;
    EthSend(0x00, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
  }
  else if (EnergyState::csEmergencyStop == State)
  {
    if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
    {
      m_dps.eStop();
      xSemaphoreGive(m_xMutex); // release the mutex
    }
    else
    {
      Serial.println("m_dps.eStop();");
    }
    uint8_t data[16];
    data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
    data[1] = 0x00; // Power OFF
    EthSend(0, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
  }
  else if (EnergyState::csTrackVoltageOff == State)
  {
    if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
    {
      m_dps.setpower(OFF);      // disable power
      xSemaphoreGive(m_xMutex); // release the mutex
    }
    else
    {
      Serial.println("m_dps.setpower(OFF);");
    }
    digitalWrite(m_enablePin1, LOW);
    digitalWrite(m_enablePin2, LOW);
    uint8_t data[16];
    data[0] = static_cast<uint8_t>(z21Interface::XHeader::LAN_X_BC_TRACK_POWER);
    data[1] = 0x00; // Power OFF
    EthSend(0, 0x07, z21Interface::Header::LAN_X_HEADER, data, true, (static_cast<uint16_t>(BcFlagShort::Z21bcAll) | static_cast<uint16_t>(BcFlagShort::Z21bcNetAll)));
  }
  z21InterfaceObserver::setPower(State);
}

void z21::notifyz21InterfaceLocoState(uint16_t Adr, uint8_t data[])
{
  if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
  {
    m_dps.getLocoData(Adr, data);
    xSemaphoreGive(m_xMutex); // release the mutex
  }
  else
  {
    Serial.println("notifyz21InterfaceLocoState");
  }

  for (auto finding = m_locos.begin(); finding != m_locos.end(); ++finding)
  { 
    if (finding->adr == Adr)
    {
      data[0] = finding->steps;
    }
  }
}

void z21::notifyz21InterfaceLocoFkt(uint16_t Adr, uint8_t type, uint8_t fkt)
{
  if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
  {
    m_dps.setLocoFunc(Adr, type, fkt);
    xSemaphoreGive(m_xMutex); // release the mutex
  }
  else
  {
    Serial.println("notifyz21InterfaceLocoFkt");
  }
}

// //--------------------------------------------------------------------------------------------
void z21::notifyz21InterfaceLocoSpeed(uint16_t Adr, uint8_t speed, uint8_t stepConfig)
{
  auto finding = m_locos.begin();
  for (; finding != m_locos.end(); ++finding)
  {
    if (finding->adr == Adr)
    {
      if (finding->steps != stepConfig)
      {
        finding->steps = stepConfig;
        saveLocoConfig();
      }
      break;
    }
  }

  if (finding == m_locos.end())// address never found
  {
    m_locos.emplace_back(ConfigLoco{Adr, stepConfig});
    saveLocoConfig();
  }

  if (xSemaphoreTake(m_xMutex, portMAX_DELAY))
  {
    switch (stepConfig)
    {
    case static_cast<uint8_t>(StepConfig::Step14):
      m_dps.setSpeed14(Adr, speed);
      break;
    case static_cast<uint8_t>(StepConfig::Step28):
      m_dps.setSpeed28(Adr, speed);
      break;
    case static_cast<uint8_t>(StepConfig::Step128):
      m_dps.setSpeed128(Adr, speed);
      break;
    default:
      m_dps.setSpeed28(Adr, speed);
      break;
    }
    xSemaphoreGive(m_xMutex); // release the mutex
  }
  else
  {
    Serial.println("notifyz21InterfaceLocoSpeed");
  }
}

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
void z21::notifyz21InterfacegetSystemInfo(uint8_t client)
{
  sendSystemInfo(client, m_currentINmA, 50000, 77); // report System State to z21Interface clients
}

void z21::handleGetLocoMode(uint16_t adr, uint8_t &mode)
{
  mode = 0; // allways DCC
}

void z21::handleSetLocoMode(uint16_t adr, uint8_t mode)
{
  // not needed
}

void z21::handleGetTurnOutMode(uint16_t adr, uint8_t &mode)
{
  mode = 0; // allways DCC
}

void z21::handleSetTurnOutMode(uint16_t adr, uint8_t mode)
{
  // not needed
}