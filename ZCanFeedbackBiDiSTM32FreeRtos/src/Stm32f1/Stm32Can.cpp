/*********************************************************************
 * Stm32Can
 *
 * based on eXoCan
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you twai redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include "Stm32f1/Stm32Can.h"

void Stm32Can::begin(IdType addrType, uint32_t brp, BusType hw)
{
    bool alt, wire;
    bool pullUp = false;

    switch (hw)
    {
    case PORTA_11_12_XCVR:
        alt = false;  // default or alternate pins
        wire = false; // bus uses a xcvr chip
        break;
    case PORTB_8_9_XCVR:
        alt = true;
        wire = false;
        break;
    case PORTA_11_12_WIRE:
        alt = false;
        wire = true;
        break;
    case PORTB_8_9_WIRE:
        alt = true;
        wire = true;
        break;
    case PORTA_11_12_WIRE_PULLUP:
        alt = false;
        wire = true;
        pullUp = true;
        break;
    case PORTB_8_9_WIRE_PULLUP:
        alt = true;
        wire = true;
        pullUp = true;
        break;

    default:
        alt = false;
        wire = false;
        break;
    }

    begin(addrType, brp, wire, alt, pullUp);
}

void Stm32Can::begin(IdType addrType, uint32_t brp, bool singleWire, bool alt, bool pullup)
{
    HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 6, 0);
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 6, 0);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 6, 0);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 6, 0);

    uint8_t inp_float = 0b0100;
    uint8_t inp_pull = 0b1000;
    uint8_t alt_out = 0b1001;
    uint8_t alt_out_od = 0b1101;

    _extIDs = addrType;

    // set up CAN IO pins
    uint8_t swMode = singleWire ? alt_out_od : alt_out;
    uint8_t inputMode = pullup ? inp_pull : inp_float;

    if (alt)
    {
        MMIO32(APB2Enr) |= (1 << 3) | (1 << 0); // enable gpioB = b3 and afio = b0 clks
        MMIO32(MAPR) |= (2 << 13);              // alt func, CAN remap to B9+B8
        MMIO32(crhB) &= 0xFFFFFF00;             // clear control bits for pins 8 & 9 of Port B
        MMIO32(crhB) |= inputMode;              // pin8 for rx, b0100 = b01xx, floating, bxx00 input
        periphBit(odrB, 8) = pullup;            // set input will pullup resistor for single wire with pullup mode
        MMIO32(crhB) |= swMode << 4;            // set output
    }
    else
    {
        MMIO32(APB2Enr) |= (1 << 2) | (1 << 0); // enable gpioA = b2 and afio = b0 clks
        MMIO32(MAPR) &= 0xffff9fff;             // CAN map to default pins, PA11/12
        MMIO32(crhA) &= 0xFFF00FFF;             // clear control bits for pins 11 & 12 of Port A
        MMIO32(crhA) |= inputMode << 12;        // pin11 for rx, b0100 = b01xx, floating, bxx00 input
        periphBit(odrA, 11) = pullup;           //
        MMIO32(crhA) |= swMode << 16;           // set output
    }
    // set up CAN peripheral
    periphBit(Rcc + 0x1C, 25) = 1;          // enable CAN1
    m_regs->MCR &= ~(1 << 1);               // exit sleep
    m_regs->MCR |= (1 << 6) | (1 << 0);     // set ABOM, init req (INRQ)
    while (((m_regs->MSR) & (1 << 0)) == 0) // wait for hw ready
        ;
    m_regs->BTR = brp; // 125K, 12/15=80% sample pt. prescale = 15
    // periphBit(TI0R, 2) = _extIDs;                      // 0 = std 11b ids, 1 = extended 29b ids
    (m_regs->MCR) &= ~(1 << 0);             // request init leave to Normal mode
    while (((m_regs->MSR) & (1 << 0)) != 0) // wait for hw
        ;
    filterMask16Init(0, 0, 0, 0, 0); // let all msgs pass to fifo0 by default
}

void Stm32Can::enableInterrupt()
{
    (m_regs->IER) |= (1 << fmpie0); // set fifo RX int enable request
    MMIO32(iser) = 1UL << 20;
}

void Stm32Can::disableInterrupt()
{
    (m_regs->IER) &= ~(1 << fmpie0);
    MMIO32(iser) = 1UL << 20;
}

void Stm32Can::filterMask16Init(int bank, int idA, int maskA, int idB, int maskB) // 16b mask filters
{
    filter16Init(bank, 0, idA, maskA, idB, maskB); // fltr 1,2 of flt bank n
}

void Stm32Can::filterList16Init(int bank, int idA, int idB, int idC, int idD) // 16b list filters
{
    filter16Init(bank, 1, idA, idB, idC, idD); // fltr 1,2,3,4 of flt bank n
}

void Stm32Can::filter16Init(int bank, int mode, int a, int b, int c, int d) // 16b filters
{
    (m_regs->FMR) |= (1 << 0);                                             // FINIT  'init' filter mode ]
    (m_regs->FA1R) &= ~(1 << bank);                                        // de-activate filter 'bank'
    (m_regs->FS1R) &= ~(1 << bank);                                        // fsc filter scale reg,  0 => 2ea. 16b
    mode ? (m_regs->FS1R) |= (1 << bank) : (m_regs->FS1R) &= ~(1 << bank); // fbm list mode = 1, 0 = mask
    (m_regs->sFilterregs[bank].FR1) = (b << 21) | (a << 5);                // fltr1,2 of flt bank n  OR  flt/mask 1 in mask mode
    (m_regs->sFilterregs[bank].FR2) = (d << 21) | (c << 5);                // fltr3,4 of flt bank n  OR  flt/mask 2 in mask mode
    (m_regs->FA1R) |= (1 << bank);                                         // activate this filter ]
    (m_regs->FMR) &= ~(1 << 0);                                            // ~FINIT  'active' filter mode ]
}
void Stm32Can::filterList32Init(int bank, uint32_t idA, uint32_t idB) // 32b filters
{
    filter32Init(bank, 1, idA, idB);
    // filter32Init(0, 1, 0x00232461, 0x00232461);
}

void Stm32Can::filterMask32Init(int bank, uint32_t id, uint32_t mask) // 32b filters
{
    filter32Init(bank, 0, id, mask);
}

void Stm32Can::filter32Init(int bank, int mode, uint32_t a, uint32_t b) // 32b filters
{
    (m_regs->FMR) |= (1 << 0);                                             // FINIT  'init' filter mode
    (m_regs->FA1R) &= ~(1 << bank);                                        // de-activate filter 'bank'
    (m_regs->FS1R) |= (1 << bank);                                         // fsc filter scale reg,  0 => 2ea. 16b,  1=>32b
    mode ? (m_regs->FS1R) |= (1 << bank) : (m_regs->FS1R) &= ~(1 << bank); // fbm list mode = 1, 0 = mask
    (m_regs->sFilterregs[bank].FR1) = (a << 3) | 4;                        // the RXID/MASK to match
    (m_regs->sFilterregs[bank].FR2) = (b << 3) | 4;                        // must replace a mask of zeros so that everything isn't passed
    (m_regs->FA1R) |= (1 << bank);                                         // activate this filter
    (m_regs->FMR) &= ~(1 << 0);                                            // ~FINIT  'active' filter mode
}

// bool Stm32Can::transmit(int txId, const void *ptr, unsigned int len)
bool Stm32Can::transmit(int txId, const void *ptr, unsigned int len)
{
    bool result{false};
    if (((m_regs->TSR) & (7 << 26)) != 0)
    {

        uint8_t freeMailbox = (m_regs->TSR & (3 << 24)) >> 24;

        if (_extIDs)
            m_regs->sTxMailBox[freeMailbox].TIR = (txId << 3) + 0b100; // // set 29b extended ID.
        else
            m_regs->sTxMailBox[freeMailbox].TIR = (txId << 21) + 0b000; // 12b std id

        m_regs->sTxMailBox[freeMailbox].TDTR = (len << 0);
        // this assumes that misaligned word access works
        m_regs->sTxMailBox[freeMailbox].TDLR = ((const uint32_t *)ptr)[0];
        m_regs->sTxMailBox[freeMailbox].TDHR = ((const uint32_t *)ptr)[1];

        m_regs->sTxMailBox[freeMailbox].TIR |= (1 << 0); // tx request
        result = true;
    }
    return result;
}

int Stm32Can::receive(volatile int &id, volatile int &fltrIdx, volatile uint8_t pData[])
{
    int len = -1;
    if ((m_regs->RF0R) & (3 << 0)) // num of msgs pending
    {
        _rxExtended = static_cast<IdType>(((m_regs->sFIFOMailBox[0].RIR) & (1 << 2)) >> 2);

        if (_rxExtended)
            id = ((m_regs->sFIFOMailBox[0].RIR) >> 3); // extended id
        else
            id = ((m_regs->sFIFOMailBox[0].RIR) >> 21);          // std id
        len = (m_regs->sFIFOMailBox[0].RDTR) & 0x0F;             // fifo data len and time stamp
        fltrIdx = ((m_regs->sFIFOMailBox[0].RDTR) >> 8) & 0xff;  // filter match index. Index accumalates from start of bank
        ((uint32_t *)pData)[0] = (m_regs->sFIFOMailBox[0].RDLR); // 4 low rx bytes
        ((uint32_t *)pData)[1] = (m_regs->sFIFOMailBox[0].RDHR); // another 4 bytes
        (m_regs->RF0R) |= (1 << 5);                              // release the mailbox
    }
    return len;
}

void Stm32Can::attachInterrupt(void func()) // copy IRQ table to SRAM, point VTOR reg to it, set IRQ addr to user ISR
{
    static uint8_t newTbl[0xF0] __attribute__((aligned(0x100)));
    uint8_t *pNewTbl = newTbl;
    int origTbl = MMIO32(vtor);
    for (int j = 0; j < 0x3c; j++) // table length = 60 integers
        MMIO32((pNewTbl + (j << 2))) = MMIO32((origTbl + (j << 2)));

    uint32_t canVectTblAdr = reinterpret_cast<uint32_t>(pNewTbl) + (36 << 2); // calc new ISR addr in new vector tbl
    MMIO32(canVectTblAdr) = reinterpret_cast<uint32_t>(func);                 // set new CAN/USB ISR jump addr into new table
    MMIO32(vtor) = reinterpret_cast<uint32_t>(pNewTbl);                       // load vtor m_regs with new tbl location
    enableInterrupt();
}

// void Stm32Can::attachInterrupt(void func()) // copy IRQ table to SRAM, point VTOR reg to it, set IRQ addr to user ISR
// {
//     static uint8_t xx[0xF0] __attribute__((aligned(0x100)));
//     uint8_t *px = xx;
//     int origTbl = MMIO32(vtor);
//     for (int j = 0; j < 0x3c; j++)
//         MMIO32((px + (j << 2))) = MMIO32((origTbl + (j << 2)));

//     uint32_t canVectTblAdr = (uint32_t)px + (36 << 2); // )USB_LP_CAN1_RX0_IRQn) + 16) << 2) isr addr location
//     MMIO32(canVectTblAdr) = (uint32_t)func;            // new vector table CAN/USB ISR jump addr
//     MMIO32(vtor) = (uint32_t)px;                       // put new location into vtor reg
// }