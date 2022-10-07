/*********************************************************************
 * ZCan Feedback BiDi Decoder for ESP32
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
#include <memory>

#include "ZCan/CanInterfaceEsp32.h"
#include "FeedbackDecoder.h"
#include <NmraDcc.h>
#include "xprintf.h"

twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_125KBITS();
std::shared_ptr<CanInterfaceEsp32> canInterface = std::make_shared<CanInterfaceEsp32>(timingConfig, GPIO_NUM_4, GPIO_NUM_5);

std::array<uint8_t, 8> trackPin1{16, 17, 18, 19, 21, 22, 23, 25};
std::array<uint8_t, 8> trackPin2{26, 27, 32, 33, 34, 35, 36, 39};

bool hasRailcom{false};

void uart_putc(uint8_t d)
{
  Serial.print((char)d);
}
// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder1("feedbackModul1", "modulConfig", xprintf, trackPin1, hasRailcom, 13, 14, true, true);
FeedbackDecoder feedbackDecoder2("feedbackModul2", "modulConfig", xprintf, trackPin2, hasRailcom, 13, 15, true, true);

NmraDcc m_dcc;
DCC_MSG m_dccPacket;
uint8_t m_dccPin{2};

void setup()
{
  Serial.begin(230000);
  xdev_out(uart_putc);

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  if (!feedbackDecoder1.setCanObserver(canInterface))
  {
    Serial.println("ERROR: No can interface defined");
  }
  if (!feedbackDecoder2.setCanObserver(canInterface))
  {
    Serial.println("ERROR: No can interface defined");
  }

  feedbackDecoder1.begin();
  feedbackDecoder2.begin();

  if (hasRailcom)
  {
    pinMode(m_dccPin, OUTPUT);
    m_dcc.pin(m_dccPin, 0);
    m_dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);
  }
}

void loop()
{
  if (hasRailcom)
  {
    m_dcc.process();
  }
  canInterface->cyclic();
  feedbackDecoder1.cyclic();
  feedbackDecoder2.cyclic();
}

void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower)
{
  Serial.printf("notifyDccAccTurnoutOutput: %u\n", Addr);
  feedbackDecoder1.callbackAccAddrReceived(Addr);
  feedbackDecoder2.callbackAccAddrReceived(Addr);
}

// void notifyDccMsg(DCC_MSG *Msg)
// {
//     Serial.print("notifyDccMsg: ");
//     for (uint8_t i = 0; i < Msg->Size; i++)
//     {
//         Serial.print(Msg->Data[i], HEX);
//         Serial.write(' ');
//     }
//     Serial.println();
// }

void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{
  //   checkRC(Addr);
  //   normalDetectON = false;
  Serial.printf("notifyDccFunc: %u\n", Addr);
  feedbackDecoder1.callbackLocoAddrReceived(Addr);
  feedbackDecoder2.callbackLocoAddrReceived(Addr);
}

void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps)
{
  //   checkRC(Addr);
  //   normalDetectON = false;
  Serial.printf("notifyDccSpeed: %u\n", Addr);
  feedbackDecoder1.callbackLocoAddrReceived(Addr);
  feedbackDecoder2.callbackLocoAddrReceived(Addr);
}