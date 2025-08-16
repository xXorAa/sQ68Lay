/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <time.h>
#include <SDL3/SDL.h>
#include <stdint.h>
#include <sys/time.h>

#include "emulator_hardware.h"
#include "emulator_logging.h"
#include "emulator_screen.h"
#include "q68_keyboard.h"
#include "q68_sd.h"
#include "q68_sound.h"
#include "spi_sdcard.h"
#include "utarray.h"

// ghost irq registers
uint8_t EMU_PC_INTR = 0;
static uint8_t q68_mc_stat = 0;
uint8_t Q68_KBD_STATUS = KBD_ISINT; // interrupt driven kbd
static uint8_t q68_q68_dmode = 0;
uint8_t EMU_Q68_MMC1_READ = 0;
uint8_t EMU_Q68_MMC1_WRIT = 0;
uint8_t EMU_Q68_MMC2_READ = 0;
uint8_t EMU_Q68_MMC2_WRIT = 0;
static Uint8 EMU_Q68_MMC1_DOUT = 0;
static Uint8 EMU_Q68_MMC2_DOUT = 0;
bool sd1en = false, sd2en = false;
static bool mmc1Clk = false;
static Uint8 mmc1Cnt = 0;
static Uint8 mmc1Dout = 0;
static Uint8 mmc1Din = 0;
static bool mmc2Clk = false;
static Uint8 mmc2Cnt = 0;
static Uint8 mmc2Dout = 0;
static Uint8 mmc2Din = 0;

static uint32_t q68_update_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return tv.tv_sec + QDOS_TIME;
}

static bool highFreq = false;
static Uint64 perfFreq;
static double perfDiv;

bool q68InitHardware(void)
{
	SDL_LogDebug(Q68_LOG_HW, "q68InitHardware");

	perfFreq = SDL_GetPerformanceFrequency();
	if (perfFreq == 0) {
		highFreq = false;
		SDL_LogError(Q68_LOG_HW, "SDL_GetPerformanceFrequency failed");
		return false;
	}

	highFreq = true;

	perfDiv = (double)perfFreq / 40000000.0;

	return true;
}

static Uint32 q68_update_hires(void)
{
	if (!highFreq) {
		return 0;
	}

	double freq = (double)SDL_GetPerformanceCounter();

	return (Uint32)(freq / perfDiv);
}

uint8_t qlHardwareRead8(unsigned int addr)
{
	switch (addr) {
	case PC_CLOCK:
		return q68_update_time() >> 24;
	case PC_CLOCK + 1:
		return (q68_update_time() >> 16) & 0xFF;
	case PC_CLOCK + 2:
		return (q68_update_time() >> 8) & 0xFF;
	case PC_CLOCK + 3:
		return q68_update_time() & 0xFF;
	case PC_IPCRD:
		return 0;
	case PC_INTR:
		return EMU_PC_INTR;
	case Q68_TIMER:
		return q68_update_hires() >> 24;
	case Q68_TIMER + 1:
		return (q68_update_hires() >> 16) & 0xFF;
	case Q68_TIMER + 2:
		return (q68_update_hires() >> 8) & 0xFF;
	case Q68_TIMER + 3:
		return q68_update_hires() & 0xFF;
	case KBD_CODE: {
		if (utarray_len(q68_kbd_queue)) {
			int *key;
			key = (int *)utarray_front(q68_kbd_queue);
			return *key;
		}

		return 0;
	}
	case KBD_STATUS:
		return Q68_KBD_STATUS;
	case Q68_MMC1_DIN:
		if (mmc1Din & 0x80) {
			return 0xFF;
		} else {
			return 0;
		}
	case Q68_MMC1_READ:
	case Q68_MMC1_READ + 1: {
		SDL_LogDebug(Q68_LOG_HW, "Q68_MMC1_READ: %2.2x",
			     EMU_Q68_MMC1_READ);
		return EMU_Q68_MMC1_READ;
	}
	case Q68_MMC2_DIN:
		if (mmc2Din & 0x80) {
			return 0xFF;
		} else {
			return 0;
		}
	case Q68_MMC2_READ:
	case Q68_MMC2_READ + 1: {
		SDL_LogDebug(Q68_LOG_HW, "Q68_MMC2_READ: %2.2x",
			     EMU_Q68_MMC2_READ);
		return EMU_Q68_MMC2_READ;
	}
	case Q68_DMODE:
		return q68_q68_dmode;
	default:
		SDL_LogDebug(Q68_LOG_HW, "read unknown: %8.8x", addr);
		break;
	}

	return 0;
}

void qlHardwareWrite8(unsigned int addr, uint8_t val)
{
	switch (addr) {
	case PC_INTR:
		EMU_PC_INTR &= ~val;
		return;
	case MC_STAT:
		q68_mc_stat = val;
		emulatorScreenChangeMode(!(val >> 3));
		return;
	case KBD_CODE:
		return;
	case KBD_UNLOCK:
		if (val & KBD_ACKN) {
			// code is acknowledged so remove it
			if (utarray_len(q68_kbd_queue)) {
				utarray_erase(q68_kbd_queue, 0, 1);
			}
			// if the queue is empty clear the interrupt
			if (!utarray_len(q68_kbd_queue)) {
				Q68_KBD_STATUS &= ~KBD_ACKN;
			}
		}
		return;
	case Q68_MMC1_CS:
		sd1en = !!val;
		SDL_LogDebug(Q68_LOG_HW, "Q68_MMC1_CS: %2.2x", val);
		break;
	case Q68_MMC1_CLK:
		if (mmc1Clk && !val && sd1en) {
			if (mmc1Cnt == 0) {
				mmc1Din = card_byte_out(0);
			} else {
				mmc1Din <<= 1;
			}

			mmc1Dout <<= 1;
			if (EMU_Q68_MMC1_DOUT) {
				mmc1Dout |= 1;
			}
			mmc1Cnt++;
			if (mmc1Cnt == 8) {
				card_byte_in(0, mmc1Dout);
				mmc1Cnt = 0;
			}
		}
		mmc1Clk = !!val;
		break;
	case Q68_MMC1_DOUT:
		EMU_Q68_MMC1_DOUT = val;
		break;
	case Q68_MMC1_WRIT:
		EMU_Q68_MMC1_WRIT = val;
		break;
	case Q68_MMC1_XFER:
		if (sd1en) {
			EMU_Q68_MMC1_READ = card_byte_out(0);
			card_byte_in(0, EMU_Q68_MMC1_WRIT);
		}
		break;
	case Q68_MMC2_CS:
		sd2en = !!val;
		SDL_LogDebug(Q68_LOG_HW, "Q68_MMC2_CS: %2.2x", val);
		break;
	case Q68_MMC2_CLK:
		if (mmc2Clk && !val && sd2en) {
			if (mmc2Cnt == 0) {
				mmc2Din = card_byte_out(1);
			} else {
				mmc2Din <<= 1;
			}

			mmc2Dout <<= 1;
			if (EMU_Q68_MMC2_DOUT) {
				mmc2Dout |= 1;
			}
			mmc2Cnt++;
			if (mmc2Cnt == 8) {
				card_byte_in(1, mmc2Dout);
				mmc2Cnt = 0;
			}
		}
		mmc2Clk = !!val;
		break;
	case Q68_MMC2_DOUT:
		EMU_Q68_MMC2_DOUT = val;
		break;
	case Q68_MMC2_WRIT:
		EMU_Q68_MMC2_WRIT = val;
		break;
	case Q68_MMC2_XFER:
		if (sd2en) {
			EMU_Q68_MMC2_READ = card_byte_out(1);
			card_byte_in(1, EMU_Q68_MMC2_WRIT);
		}
		break;
	case Q68_SOUND_RIGHT:
		q68PlayByte(1, val);
		break;
	case Q68_SOUND_LEFT:
		q68PlayByte(0, val);
		break;
	case Q68_DMODE:
		q68_q68_dmode = val;
		emulatorScreenChangeMode(val & 7);
		return;
	default:
		SDL_LogDebug(Q68_LOG_HW, "write unknown: %8.8x %2.2x", addr,
			     val);
		break;
	}
}
