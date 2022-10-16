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

#pragma once

/* Includes ------------------------------------------------------------------*/
//#include "stm32f1xx_hal.h"
#include "Helper/micros.h"
#include "Helper/xprintf.h"
#include "main.h"

class Dcc
{
public:
    typedef enum
    {
        DCC_ADDR_SHORT, /** Short address is used. The range is 0 to 127. */
        DCC_ADDR_LONG,  /** Long Address is used. The range is 1 to 10239 */
    } AddrType;

public:
    Dcc(bool (*pinIsSet)(void));
    ~Dcc();
    void begin(void);
    void cyclic(void);
    void interruptHandler(void);

    void setNotifyDccAccTurnoutOutput(void (*notifyDccAccTurnoutOutput)(uint16_t Addr, uint8_t Direction, uint8_t OutputPower));
    void setNotifyDccLoco(void (*notifyDccLoco)(uint16_t addr, Dcc::AddrType addrType));

private:
    typedef enum
    {
        Idle,       // warte auf DCC Daten
        NewMessage // neue DCC Message
    } State;

    State m_state;

    uint8_t m_numberOfOne;   // Zähle die high so das man eine Präambel erkennen kann (min. 10 high)
    bool m_getData;   // Es werden DCC Daten gelesen und im Array data abgespeichert
    bool m_dataReady; // Es wurden DCC Daten vollständig eingelesen
    uint8_t m_dataBuf[4];        // eingelesene Byte-Werte nach einer Präambel
    uint8_t m_dataLength; // Position in data wo gerade der nächste Wert hinkommt
    uint8_t m_bitCount;   // Zähle Bits, nach 8 Bit wurde ein Byte gelesen und setzte dann die Variable auf 0
    uint8_t m_xorChecksum;       // xor Prüfsumme
    uint16_t m_receivedAdr;   // Adresse

    volatile unsigned long m_timeSinceLastRisingEdgeINus = 0; // Time to check DCC Data "1" or "0"

    bool (*m_pinIsSet)(void);

    void (*m_notifyDccAccTurnoutOutput)(uint16_t Addr, uint8_t Direction, uint8_t OutputPower);

    void (*m_notifyDccLoco)(uint16_t addr, Dcc::AddrType addrType);

    void processData(void);
};