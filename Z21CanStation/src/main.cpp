/*********************************************************************
 * Z21 ESP
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

#include <Arduino.h>
// For Debugging active flag and read out via serial:
#define DEBUG

#include <EEPROM.h>

#include "WebService.h"
#include "ZCan/CanInterfaceEsp32.h"
#include "z21/UdpInterfaceEsp32.h"
#include "z21.h"
#include "xprintf.h"

#include <AsyncUDP.h>

void uart_putc(uint8_t d)
{
  Serial.print((char)d);
}

twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_125KBITS();
std::shared_ptr<CanInterfaceEsp32> canInterface = std::make_shared<CanInterfaceEsp32>(timingConfig, GPIO_NUM_4, GPIO_NUM_5);

const uint16_t hash{0};
const uint32_t serialNumber{0xFFFFFFF0};
const uint16_t swVersion{0x0142};
const int16_t z21Port{21105};

std::shared_ptr<UdpInterfaceEsp32> udpInterface = std::make_shared<UdpInterfaceEsp32>(30, z21Port, false);
z21::ConfigDccStation configDccStation{25, 26, 34, 2.0};//2 Ampere max current
z21 centralStation(configDccStation, hash, serialNumber, z21Interface::HwType::Z21_XL, swVersion, xprintf, true, false, false);

AsyncUDP udp;

/**********************************************************************************/
void setup()
{
  Serial.begin(230000);
  xdev_out(uart_putc);

  AutoConnectConfig configAutoConnect;

  String idOfEsp = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  while (idOfEsp.length() < 4)
  {
    idOfEsp += "0";
  }
  Serial.printf("ID of chip: ");
  Serial.println(idOfEsp);

  configAutoConnect.ota = AC_OTA_BUILTIN;
  configAutoConnect.apid = "z21AP-" + idOfEsp;
  configAutoConnect.psk = idOfEsp + idOfEsp;
  configAutoConnect.apip = IPAddress(192, 168, 0, 111); // Sets SoftAP IP address
  configAutoConnect.netmask = IPAddress(255, 255, 255, 0);
  configAutoConnect.channel = random(1, 12);
  Serial.printf("Wifi Channel:%d\n", configAutoConnect.channel);
  configAutoConnect.title = "z21";
  configAutoConnect.beginTimeout = 15000;
  configAutoConnect.autoReset = false;

  //configAutoConnect.homeUri = "/_ac";

  // reconnect with last ssid in handleClient
  configAutoConnect.autoReconnect = true;
  configAutoConnect.reconnectInterval = 15;

  configAutoConnect.portalTimeout = 1;

  configAutoConnect.immediateStart = true;
  configAutoConnect.autoRise = true;
  configAutoConnect.retainPortal = true;

  WebService *webService = WebService::getInstance();

  if (nullptr != webService)
  {
    webService->begin(configAutoConnect);
  }

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  if (!centralStation.setCanObserver(canInterface))
  {
    Serial.println("ERROR: No can interface defined");
  }

  if (nullptr != udpInterface.get())
  {
    udpInterface->begin();
  }

  if (!centralStation.setUdpObserver(udpInterface))
  {
    Serial.println("ERROR: No udp interface defined");
  }

  centralStation.begin();

  if (udp.listen(34472))
  {
    udp.onPacket([](AsyncUDPPacket packet)
                 {
      Serial.print("=>");
      Serial.print(packet.remoteIP());
      for(int i=0;i<packet.length();i++)
      {
        Serial.print(" ");
        Serial.print(packet.data()[i], HEX);
      }
      Serial.print("\n");
      for (int i = 0; i < packet.length(); i++)
      {
        Serial.print(" ");
        Serial.print((char)packet.data()[i]);
      }
      Serial.print("\n"); });
  }
  Serial.println("OK"); // start - reset serial receive Buffer
}

/**********************************************************************************/
void loop()
{
  WebService *webService = WebService::getInstance();
  if (nullptr != webService)
  {
    webService->cyclic();
  }
  canInterface->cyclic();
  udpInterface->cyclic();
  centralStation.cyclic();
  // delayMicroseconds(1);
}