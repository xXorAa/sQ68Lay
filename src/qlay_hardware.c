/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <sys/time.h>

#include "emulator_hardware.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "qlay_io.h"
#include "qlay_sound.h"

// ghost irq registers
Uint8 EMU_PC_INTR = 0;
Uint8 EMU_PC_INTR_MASK = 0;
Uint8 EMU_MC_STAT = 0;
Uint8 EMU_PC_TRAK1 = 0;
Uint8 EMU_PC_TRAK2 = 0;

// ghost RTC register
Uint32 EMU_PC_CLOCK = 0;

// qsound state
bool qsound_enabled = false;
Uint32 qsound_addr = 0;

void qlayInitialiseTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	EMU_PC_CLOCK = tv.tv_sec + QDOS_TIME;
}

void qlayInitialiseQsound(void)
{
	const char *qsound = emulatorOptionString("qsound");

	// no qsound set
	if (!qsound) {
		return;
	}

	qsound_addr = SDL_strtol(qsound, NULL, 16);

	if (qsound_addr != 0) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "QSound address 0x%X",
			    qsound_addr);
		qsound_enabled = true;
	}
}

Uint8 qlHardwareRead8(unsigned int addr)
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

void qlHardwareWrite8(unsigned int addr, Uint8 val)
{
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
		Uint8 tmp;
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

enum { QSOUND_PA, QSOUND_CA, QSOUND_PB, QSOUND_CB };
#define QSOUND_REG_BIT (1 << 0)
#define QSOUND_LATCH_BIT (1 << 2)
static Uint8 qsound_pa_last;
static Uint8 qsound_pb_last;
static Uint8 qsound_reg_num;

void qsoundWrite(Uint32 address, Uint8 val)
{
	switch (address % 4) {
	case QSOUND_PA:
		SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
			     "QSound PA data 0x%X", val);
		qsound_pa_last = val;
		break;
	case QSOUND_PB:
		SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
			     "QSound PB data 0x%X", val);
		if ((qsound_pb_last & QSOUND_LATCH_BIT) &&
		    !(val & QSOUND_LATCH_BIT)) {
			if (qsound_pb_last & QSOUND_REG_BIT) {
				SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
					     "QSound Latching reg_num 0x%2.2X",
					     qsound_pa_last);
				qsound_reg_num = qsound_pa_last;
			} else {
				SDL_LogTrace(
					SDL_LOG_CATEGORY_APPLICATION,
					"QSound Latching reg_val 0x%2.2X 0x%2.2X",
					qsound_reg_num, qsound_pa_last);

				qlaySetAYRegister(qsound_reg_num,
						  qsound_pa_last);
			}
		}
		qsound_pb_last = val;
		break;
	default:
		SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
			     "QSound control 0x%X", val);
	}
}
