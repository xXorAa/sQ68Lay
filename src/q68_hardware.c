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
#include "emulator_screen.h"
#include "q68_keyboard.h"
#include "q68_sd.h"
#include "utarray.h"

// ghost irq registers
uint8_t EMU_PC_INTR = 0;
static uint8_t q68_mc_stat = 0;
uint8_t Q68_KBD_STATUS = KBD_ISINT; // interrupt driven kbd
static uint8_t q68_q68_dmode = 0;

static uint32_t q68_update_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return tv.tv_sec + QDOS_TIME;
}

static uint32_t q68_update_hires(void)
{
	uint64_t freq = SDL_GetPerformanceFrequency();

	return SDL_GetPerformanceCounter() / (freq / 40000000);
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
	case Q68_MMC1_READ:
	case Q68_MMC1_READ + 1: {
		uint8_t res = q68ProcessSDResponse(0);
		return res;
	}
	case Q68_DMODE:
		return q68_q68_dmode;
	default:
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
	case Q68_MMC1_WRIT:
		q68ProcessSDCmd(0, val);
		break;
	case Q68_MMC1_XFER:
		break;
	case Q68_DMODE:
		q68_q68_dmode = val;
		emulatorScreenChangeMode(val & 7);
		return;
	default:
		break;
	}
}
