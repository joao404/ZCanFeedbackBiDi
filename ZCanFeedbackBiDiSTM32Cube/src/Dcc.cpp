/*********************************************************************
 * Dcc
 *
 * Based on code of Philipp Gartow
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

#include "Dcc.h"
#include <cstring>
#include "Helper/xprintf.h"

Dcc::Dcc(bool (*pinIsSet)(void))
    : m_state(Idle),
      m_numberOfOne(0),
      m_getData(false),
      m_dataReady(false),
      m_dataLength(0),
      m_bitCount(0),
      m_xorChecksum(0),
      m_receivedAdr(0),
      m_pinIsSet(pinIsSet),
      m_notifyDccAccTurnoutOutput(nullptr),
      m_notifyDccLoco(nullptr)
{
  memset(m_dataBuf, 0, sizeof(m_dataBuf));
}

Dcc::~Dcc()
{
}

void Dcc::begin(void)
{
  m_timeSinceLastRisingEdgeINus = micros();
}

void Dcc::cyclic(void)
{
  switch (m_state)
  {
  case Idle:
    // decode received DCC Data!
    // if (true == updateData())
    // {
    //   m_state = NewMessage;
    // }
    // reagiere auf CAN-Nachrichten

    // Auswerten, ob Gleis belegt ist

    break;
  case NewMessage:
    // delayMicroseconds(450);//wait for Railcom
    break;
  default:
    m_state = Idle;
    break;
  }
}

void Dcc::interruptHandler(void)
{
  unsigned long currentTimeINus = micros();
  if (m_pinIsSet())
  {                                                  // digitalRead(DCCPIN) == HIGH) {
    m_timeSinceLastRisingEdgeINus = currentTimeINus; // 16MHz = resolution of four microseconds
    return;
  }

  if (m_getData == true)
  {
    m_bitCount += 1; // Abzählen der Bytes, also jeweils 8 Bit
  }

  if (currentTimeINus <= (m_timeSinceLastRisingEdgeINus + 85))
  {                     // 1-Bit gelesen; over 68us
    m_numberOfOne += 1; // Zählen der 1-Bit für Präambel erforderlich
    if (m_getData == true && m_bitCount <= 8)
    {                                                     // eingelesenen Bitwert im Array Speichern
      m_dataBuf[m_dataLength] |= (1 << (8 - m_bitCount)); // Speichert das ein 1-Bit gelesen wurde
    }
    if (m_bitCount > 8)
    { // End-Bit gelesen.
      m_bitCount = 0;
      m_numberOfOne = 0;
      m_getData = false; // Stop des Einlesen der Daten
      // XOR Prüfen:
      if (m_dataBuf[m_dataLength] == m_xorChecksum && m_dataBuf[0] != 0xFF) // Prüfen von XOR und letztes Byte
      {
        m_dataReady = true; // Fertig, Daten Auswerten!
        processData();
        m_dataReady = false;
      }
    }
        // xprintf("1");
  } // Ende 1-Bit
  else
  { // 0-Bit gelesen
    // xprintf("0");
    if (m_getData == true && m_bitCount <= 8)
    {                                                      // eingelesenen Bitwert im Array Speichern
      m_dataBuf[m_dataLength] &= ~(1 << (8 - m_bitCount)); // Speichert das ein 0-Bit gelesen wurde
    }
    if (m_numberOfOne > 10)
    {                    // Präambel erkannt ab hier Daten lesen. (Es wurden mind. 10 HIGH erkannt)
      m_getData = true;  // Einlesen der Bitwerte in Array aktivieren.
      m_dataLength = 0;  // Position im Array an der die Bitwerte gespeichert werden.
      m_bitCount = 0;    // beginne Bits zu zählen. Auswertung von 1 Byte Blöcken ermöglichen nach 8 Bit.
      m_xorChecksum = 0; // XOR zurücksetzten
    }
    if (m_bitCount > 8)
    { // Null-Bit gelesen. (Immer nach 1 Byte)
      m_bitCount = 0;
      m_xorChecksum = m_xorChecksum ^ m_dataBuf[m_dataLength]; // XOR bestimmen!
      m_dataLength += 1;                                       // Bitweise im Array weitergehen.
    }
    m_numberOfOne = 0; // Es wurde ein 0-Bit gelesen, lösche Anzahl gelesener 1-Bit
  }                    // Ende 0-Bit
}

void Dcc::setNotifyDccAccTurnoutOutput(void (*notifyDccAccTurnoutOutput)(uint16_t Addr, uint8_t Direction, uint8_t OutputPower))
{
  m_notifyDccAccTurnoutOutput = notifyDccAccTurnoutOutput;
}

void Dcc::setNotifyDccLoco(void (*notifyDccLoco)(uint16_t addr, Dcc::AddrType addrType))
{
  m_notifyDccLoco = notifyDccLoco;
}

void Dcc::processData(void)
{
  xprintf("%d %d %d %d\n", m_dataBuf[0], m_dataBuf[1], m_dataBuf[2], m_dataBuf[3]);
  /*
  Beschreibung der Einträge im data Array:
    data[0]   : 1. Byte nach der Präambel (Adresse)
    Bit 8     : Null-Bit
    data[1]   : 2. Byte (long Adresse oder Daten)
    Bit 17    : Null-Bit
    data[2]   : 3. Byte (bei datalength == 26 => CRC)
    Bit 26    : Null-Bit (bei datalength == 26 => End-Bit)
    data[3]   : 4. Byte (wenn vorhanden, long Adr. oder long Data)
    Bit 35    : End-Bit (wenn vorhanden, long Adr. oder long Data)
    data[4]   : 5. Byte (wenn vorhanden, long Adr. und long Data)
    Bit 44    : End-Bit (wenn vorhanden, long Adr. und long Data)


Instructions packets using Direct CV Addressing are 4 byte packets of the format:
//long-preamble 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1
    CC=10 Bit Manipulation
    CC=01 Verify byte
    CC=11 Write byte

    */

  // 1.1 Bestimmen kurzen Adresse (bis 127)
  if (!(m_dataBuf[0] & 0x80))
  {
    m_receivedAdr = m_dataBuf[0];
    if (m_receivedAdr == 0xFF || m_receivedAdr == 0) // idle
    {
      m_receivedAdr = 0;
    }
    else
    {
      if (m_notifyDccLoco)
      {
        m_notifyDccLoco(m_receivedAdr, AddrType::DCC_ADDR_SHORT);
      }
    }
  }
  // 1.2 Bestimmen der langen Adresse mit 14 Bits also 16384
  else if (m_dataBuf[0] >> 6 == 0x03)
  {
    m_receivedAdr = (m_dataBuf[0] & 0x3F) << 8;
    m_receivedAdr += m_dataBuf[1];
    if (m_receivedAdr == 16128)
    { // idle
      m_receivedAdr = 0;
    }
    else
    {
      if (m_notifyDccLoco)
      {
        m_notifyDccLoco(m_receivedAdr, AddrType::DCC_ADDR_LONG);
      }
    }
  }
  // 1.3 Bestimmen der Adresse Weichenadrresse
  else if (((m_dataBuf[0] >> 6) == 0x02) && ((m_dataBuf[1] >> 7) == 0x01))
  {
    m_receivedAdr = (((~m_dataBuf[1]) & 0x70) << 2) | (m_dataBuf[0] & 0x3F); // Adresse
    m_receivedAdr = (m_receivedAdr << 2) | ((m_dataBuf[1] >> 0x01) & 0x03);  // Port bestimmen, bis zu 4 möglich
    m_receivedAdr = m_receivedAdr + 1;
    // #if defined(SHIFTADR)
    // dccAdr = dccAdr - 4;
    // #endif
    if (m_notifyDccAccTurnoutOutput)
    {
      m_notifyDccAccTurnoutOutput(m_receivedAdr, m_dataBuf[1], 0);
    }
  }
  else
  {
    m_receivedAdr = 0;
  }
}
