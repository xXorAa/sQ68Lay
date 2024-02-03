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
#include "qlay_ipc.h"

// ghost irq registers
uint8_t EMU_PC_INTR = 0;
uint8_t EMU_PC_INTR_MASK = 0;
uint8_t EMU_MC_STAT = 0;
uint8_t EMU_PC_TRAK1 = 0;
uint8_t EMU_PC_TRAK2 = 0;

static uint32_t qlayUpdateTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return tv.tv_sec + QDOS_TIME;
}

uint8_t qlHardwareRead8(unsigned int addr)
{
	switch (addr) {
	case PC_CLOCK:
		return qlayUpdateTime() >> 24;
	case PC_CLOCK + 1:
		return (qlayUpdateTime() >> 16) & 0xFF;
	case PC_CLOCK + 2:
		return (qlayUpdateTime() >> 8) & 0xFF;
	case PC_CLOCK + 3:
		return qlayUpdateTime() & 0xFF;
	case PC_IPCRD:
	case PC_TRAK1:
	case PC_TRAK2:
		return readQLHw(addr);
	case PC_INTR:
		return EMU_PC_INTR;
	default:
		break;
	}

	return 0;
}

void qlHardwareWrite8(unsigned int addr, uint8_t val)
{
	//std::cout << "HWW8: " << std::hex << addr << "," << val << std::endl;
	switch (addr) {
	case PC_TCTRL:
		wrZX8302(val);
		return;
	case PC_IPCWR:
		wr8049(val);
		return;
	case PC_MCTRL:
		wrmdvcntl(val);
		return;
	case PC_INTR:
		EMU_PC_INTR_MASK = (val & 0xE0);
		EMU_PC_INTR &= 0x1F;
		EMU_PC_INTR &= ~val;
		return;
	case MC_STAT:
		EMU_MC_STAT = val;
		emulatorScreenChangeMode(!(val >> 3));
		return;
	case PC_TRAK1:
		writeMdvSer(val);
		return;
	default:
		break;
	}
}
