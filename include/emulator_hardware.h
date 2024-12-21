/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */
#pragma once

#include <stdint.h>

#ifndef EMULATOR_HARDWARE_H
#define EMULATOR_HARDWARE_H

#define BIT(nr) (1UL << (nr))

// time offset for RTC
#define QDOS_TIME ((9 * 365 + 2) * 86400)

// QL IO space
#define PC_CLOCK 0x18000

#define PC_TCTRL 0x18002

#define PC_IPCWR 0x18003

#define PC_MCTRL 0x18020
// read1
#define PC__CTS2 BIT(5) // CTS on port 2 (set if ser2 transmit held up)
#define PC__DTR1 BIT(4) // DTR on port 1 (set if ser1 transmit held up)
#define PC__GAP BIT(3) // gap : set normally, or gap is present on running mdv
#define PC__RXRD BIT(2) // microdrive read buffer ready
#define PC__TXFL BIT(1) //xmit buffer full(mdv or ser)
// write
#define PC__ERASE BIT(3) // microdrive erase
#define PC__WRITE BIT(2) // microdrive write
#define PC__SCLK BIT(1) // microdrive select clock bit
#define PC__SEL BIT(0) // microdrive select bit

#define PC_IPCRD 0x18020

#define PC_INTR 0x18021
#define PC_INTRG BIT(0)
#define PC_INTRI BIT(1)
#define PC_INTRT BIT(2)
#define PC_INTRF BIT(3)
#define PC_INTRE BIT(4)
#define PC_MASKG BIT(5)
#define PC_MASKI BIT(6)
#define PC_MASKT BIT(7)

#define PC_TRAK1 0x18022

#define PC_TRAK2 0x18023

#define MC_STAT 0x18063 // status register address
//                         clear   set
#define MC__BLNK BIT(1) // normal  blank   if set, the rest are irrelevant!
#define MC__M256 BIT(3) // 512/4   256/8   pixels per line / colours per pixel
#define MC__NTSC BIT(6) // PAL625  NTSC495 scan lines (512/384 displayed)
#define MC__SCRN BIT(7) // $20000  $28000  screen base address (scr0/scr1)

// Q68 IO space
#define Q68_TIMER 0x1c060

#define KBD_CODE 0x1c140

#define KBD_UNLOCK 0x1c144
#define KBD_ACKN (1 << 0)

#define KBD_STATUS 0x1c148
#define KBD_RCV (1 << 0)
#define KBD_ISINT (1 << 7)

// Q68 SDHC
#define Q68_MMC1_CS 0x1c300
#define Q68_MMC1_CLK 0x1c304
#define Q68_MMC1_DIN 0x1c308
#define Q68_MMC1_DOUT 0x1c30c
#define Q68_MMC1_READ 0x1c310
#define Q68_MMC1_WRIT 0x1c314
#define Q68_MMC1_XFER 0x1c318

#define Q68_DMODE 0xff000018

uint8_t qlHardwareRead8(unsigned int addr);
void qlHardwareWrite8(unsigned int addr, uint8_t val);

/* qlay RTC initialise */
void qlayInitialiseTime(void);

/* Shadow registers */
extern uint8_t EMU_PC_INTR;
extern uint8_t EMU_PC_INTR_MASK;
extern uint8_t Q68_KBD_STATUS;
extern uint8_t EMU_PC_TRAK1;
extern uint8_t EMU_PC_TRAK2;
extern uint32_t EMU_PC_CLOCK;

#endif /* EMULATOR_HARDWARE_H */
