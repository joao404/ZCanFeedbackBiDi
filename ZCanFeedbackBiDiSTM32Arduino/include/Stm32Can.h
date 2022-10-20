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

#include "Arduino.h"

class Stm32Can
{
public:
  // register addresses
  constexpr static uint32_t CanBase = 0x40006400;

  constexpr static uint32_t MCR = CanBase + 0x000;  // master cntrl
  constexpr static uint32_t MSR = CanBase + 0x004;  // rx status
  constexpr static uint32_t TSR = CanBase + 0x008;  // tx status
  constexpr static uint32_t RF0R = CanBase + 0x00C; // rx fifo 0 info reg

  constexpr static uint32_t IER = CanBase + 0x014; // interrupt enable

  constexpr static uint32_t BTR = CanBase + 0x01C; // bit timing and rate

  constexpr static uint32_t TI0R = CanBase + 0x180;  // tx mailbox id
  constexpr static uint32_t TDT0r = CanBase + 0x184; // tx data len and time stamp
  constexpr static uint32_t TDL0r = CanBase + 0x188; // tx mailbox data[3:0]
  constexpr static uint32_t TDH0r = CanBase + 0x18C; // tx mailbox data[7:4]

  constexpr static uint32_t RI0r = CanBase + 0x1B0;  // rx fifo id reg
  constexpr static uint32_t RDT0r = CanBase + 0x1B4; // fifo data len and time stamp
  constexpr static uint32_t RDL0r = CanBase + 0x1B8; // rx fifo data low
  constexpr static uint32_t RDH0r = CanBase + 0x1BC; // rx fifo data high

  constexpr static uint32_t fmr = CanBase + 0x200;   // filter master reg
  constexpr static uint32_t fm1r = CanBase + 0x204;  // filter mode reg
  constexpr static uint32_t fs1r = CanBase + 0x20C;  // filter scale reg, 16/32 bits
  constexpr static uint32_t ffa1r = CanBase + 0x214; // filter FIFO assignment
  constexpr static uint32_t fa1r = CanBase + 0x21C;  // filter activation reg
  constexpr static uint32_t fr1 = CanBase + 0x240;   // id/mask acceptance reg1
  constexpr static uint32_t fr2 = CanBase + 0x244;   // id/mask acceptance reg2

  constexpr static uint32_t scsBase = 0xE000E000UL;        // System Control Space Base Address
  constexpr static uint32_t nvicBase = scsBase + 0x0100UL; // NVIC Base Address
  constexpr static uint32_t iser = nvicBase + 0x000;       //  NVIC interrupt set (enable)
  constexpr static uint32_t icer = nvicBase + 0x080;       // NVIC interrupt clear (disable)

  constexpr static uint32_t scbBase = scsBase + 0x0D00UL;
  constexpr static uint32_t vtor = scbBase + 0x008;

  // GPIO/AFIO Regs
  constexpr static uint32_t AfioBase = 0x40010000UL;
  constexpr static uint32_t MAPR = AfioBase + 0x004; // alternate pin function mapping

  constexpr static uint32_t GpioABase = 0x40010800UL; // port A
  constexpr static uint32_t crhA = GpioABase + 0x004; // cntrl reg for port b
  constexpr static uint32_t odrA = GpioABase + 0x00c; // output data reg

  constexpr static uint32_t GpioBBase = GpioABase + 0x400; // port B
  constexpr static uint32_t crhB = GpioBBase + 0x004;      // cntrl reg for port b
  constexpr static uint32_t odrB = GpioBBase + 0x00c;      // output data reg

  // Clock
  constexpr static uint32_t Rcc = 0x40021000UL;
  constexpr static uint32_t RccBase = 0x40021000UL;
  constexpr static uint32_t APB1Enr = RccBase + 0x01c;
  constexpr static uint32_t APB2Enr = RccBase + 0x018;

// Helpers
#define MMIO32(x) (*(volatile uint32_t *)(x))
#define MMIO16(x) (*(volatile uint16_t *)(x))
#define MMIO8(x) (*(volatile uint8_t *)(x))

  static inline volatile uint32_t &periphBit(uint32_t addr, int bitNum) // peripheral bit tool
  {
    return MMIO32(0x42000000 + ((addr & 0xFFFFF) << 5) + (bitNum << 2)); // uses bit band memory
  }

#define INRQ MCR, 0
#define INAK MSR, 0
#define FINIT fmr, 0
#define fmpie0 1 // rx interrupt enable on rx msg pending bit

  typedef enum : uint8_t
  {
    PORTA_11_12_XCVR,
    PORTB_8_9_XCVR,
    PORTA_11_12_WIRE,
    PORTB_8_9_WIRE,
    PORTA_11_12_WIRE_PULLUP,
    PORTB_8_9_WIRE_PULLUP
  } BusType;

  typedef enum : bool
  {
    STD_ID_LEN,
    EXT_ID_LEN
  } IdType;

  typedef union
  {
    uint8_t bytes[8] = {0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff};
    int16_t int16[4];
    int32_t int32[2];
    int64_t int64;
  } MSG;

  typedef struct
  {
    int txMsgID = 0x68; // volatile
    IdType idLen = STD_ID_LEN;
    uint8_t txMsgLen = 0x08;
    MSG txMsg;
    // uint8_t txMsg[8];
    BusType busConfig = PORTA_11_12_XCVR;
    uint32_t txDly = 5000;
  } msgFrm;

public:
  Stm32Can(IdType addrType = STD_ID_LEN, uint32_t brp = (2 << 20) | (13 << 16) | (14 << 0), BusType hw = PORTA_11_12_XCVR)
  {
    begin(addrType, brp, hw);
  }
  void begin(IdType addrType = STD_ID_LEN, uint32_t brp = (2 << 20) | (13 << 16) | (14 << 0), BusType hw = PORTA_11_12_XCVR);
  void begin(IdType addrType, uint32_t brp, bool singleWire, bool alt, bool pullup);
  void enableInterrupt();
  void disableInterrupt();
  void filterMask16Init(int bank, int idA = 0, int maskA = 0, int idB = 0, int maskB = 0x7ff); // 16b mask filters
  void filterList16Init(int bank, int idA = 0, int idB = 0, int idC = 0, int idD = 0);         // 16b list filters
  void filterMask32Init(int bank, u_int32_t id = 0, u_int32_t mask = 0);
  void filterList32Init(int bank, u_int32_t idA = 0, u_int32_t idB = 0); // 32b filters
  bool transmit(int txId, const void *ptr, unsigned int len);
  // int receive(volatile int *id, volatile int *fltrIdx, volatile void *pData);
  int receive(volatile int &id, volatile int &fltrIdx, volatile uint8_t pData[]);
  void attachInterrupt(void func());
  bool getSilentMode() { return MMIO32(BTR) >> 31; }
  void setAutoTxRetry(bool val = true) { periphBit(MCR, 4) = !val; } // &= 0xffffffef | retry << 4;}     // if tx isn't ACK'd don't retry
  // void setSilentMode(bool silent) { MMIO32(BTR) &= 0x7fffffff | silent << 31; } // bus listen only
  void setSilentMode(bool val) { periphBit(BTR, 31) = val; }
  IdType getIDType() { return _extIDs; }
  IdType getRxIDType() { return _rxExtended; }
  ~Stm32Can() {}

  // uint8_t rxMsgCnt = 0; //num of msgs in fifo0
  // uint8_t rxFull = 0;
  // uint8_t rxOverflow = 0;

  uint8_t getRxMsgFifo0Cnt() { return MMIO32(RF0R) & (3 << 0); } // num of msgs
  uint8_t getRxMsgFifo0Full() { return MMIO32(RF0R) & (1 << 3); }
  uint8_t getRxMsgFifo0Overflow() { return MMIO32(RF0R) & (1 << 4); } // b4

  volatile int rxMsgLen = -1; // CAN parms
  volatile int id, fltIdx;
  volatile MSG rxData; // was uint8_t

protected:
private:
  IdType _extIDs = STD_ID_LEN;
  IdType _rxExtended;
  void filter16Init(int bank, int mode, int a = 0, int b = 0, int c = 0, int d = 0); // 16b filters
  void filter32Init(int bank, int mode, u_int32_t a, u_int32_t b);                   // 32b filters
};