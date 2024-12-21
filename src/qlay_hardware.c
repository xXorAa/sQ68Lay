/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>
#include <stdint.h>
#include <sys/time.h>

#include "emulator_hardware.h"
#include "emulator_screen.h"
#include "m68k.h"
#include "qlay_io.h"

// ghost irq registers
uint8_t EMU_PC_INTR = 0;
uint8_t EMU_PC_INTR_MASK = 0;
uint8_t EMU_MC_STAT = 0;
uint8_t EMU_PC_TRAK1 = 0;
uint8_t EMU_PC_TRAK2 = 0;

// ghost RTC register
uint32_t EMU_PC_CLOCK = 0;

void qlayInitialiseTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	EMU_PC_CLOCK = tv.tv_sec + QDOS_TIME;
}

uint8_t qlHardwareRead8(unsigned int addr)
{
	switch (addr) {
	case PC_CLOCK: // 18000
		return EMU_PC_CLOCK >> 24;
	case PC_CLOCK + 1:
		return (EMU_PC_CLOCK >> 16) & 0xFF;
	case PC_CLOCK + 2:
		return (EMU_PC_CLOCK >> 8) & 0xFF;
	case PC_CLOCK + 3:
		return EMU_PC_CLOCK & 0xFF;
	case PC_IPCRD: // 18020
		return readQLHw(addr);
	case PC_INTR: // 18021
		return EMU_PC_INTR;
	case PC_TRAK1: // 18022
	case PC_TRAK2: // 18023
		return readQLHw(addr);
	case MC_STAT: // 18063
		return EMU_MC_STAT;
	default:
		break;
	}

	return 0;
}

void qlHardwareWrite8(unsigned int addr, uint8_t val)
{
	//std::cout << "HWW8: " << std::hex << addr << "," << val << std::endl;
	switch (addr) {
	case PC_CLOCK:
		EMU_PC_CLOCK = 0;
		return;
	case PC_CLOCK + 1:
		switch (val & 0x1E) {
		case 0x0E:
			EMU_PC_CLOCK += 0x01000000;
			break;
		case 0x16:
			EMU_PC_CLOCK += 0x00010000;
			break;
		case 0x1A:
			EMU_PC_CLOCK += 0x00000100;
			break;
		case 0x1C:
			EMU_PC_CLOCK += 0x00000001;
			break;
		}
		return;
	case PC_TCTRL: // 18002
		wrZX8302(val);
		return;
	case PC_IPCWR: // 18003
		wr8049(val);
		return;
	case PC_MCTRL: // 18020
		wrmdvcntl(val);
		return;
	case PC_INTR: // 18021
	{
		uint8_t tmp;
		EMU_PC_INTR_MASK = (val & 0xE0);
		tmp = val & 0x1F;
		EMU_PC_INTR &= ~tmp;
		return;
	}
	case PC_TRAK1: // 18022
		writeMdvSer(val);
		return;
	case MC_STAT: // 18063
		EMU_MC_STAT = val;
		if (val & MC__SCRN) {
			emulatorSecondScreen = true;
		} else {
			emulatorSecondScreen = false;
		}
		if (val & MC__M256) {
			emulatorScreenChangeMode(0);
		} else {
			emulatorScreenChangeMode(1);
		}
		return;
	default:
		break;
	}
}
