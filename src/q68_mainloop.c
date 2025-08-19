/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>

#include "emulator_events.h"
#include "emulator_files.h"
#include "emulator_hardware.h"
#include "emulator_keyboard.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "m68k.h"
#include "q68_disk.h"
#include "q68_hooks.h"
#include "q68_keyboard.h"
#include "q68_sd.h"
#include "q68_sound.h"
#include "spi_sdcard.h"
#include "utarray.h"

typedef struct {
	uint64_t screenTick;
	uint64_t screenThen;
} emulator_state_t;

uint32_t msClk = 0;
uint32_t msClkNextEvent = 0;

void *emulatorInitEmulation(void)
{
	const char *smsqe = emulatorOptionString("smsqe");
	const char *sysrom = emulatorOptionString("sysrom");

	uint32_t initPc = 0;

	q68InitHardware();
	q68InitSound();
	q68InitKeyb();
	q68DiskInitialise();

	if (strlen(smsqe) > 0) {
		emulatorLoadFile(smsqe,
				 &emulatorMemorySpace()[Q68_SMSQE_WIN_ADDR], 0);
		initPc = Q68_SMSQE_WIN_ADDR;
	} else if (strlen(sysrom) > 0) {
		emulatorLoadFile(sysrom,
				 &emulatorMemorySpace()[Q68_SYSROM_ADDR], 0);
		romProtect = true;
	} else {
		initPc = q68DiskReadSMSQE();

		if (initPc == Q68_INVALID) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				     "Failed to load SMSQE or SYSROM");
			return NULL;
		}
	}

	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_init();
	m68k_pulse_reset();
	if (initPc) {
		m68k_set_reg(M68K_REG_PC, initPc);
	}

	emulator_state_t *emu_state = SDL_calloc(1, sizeof(emulator_state_t));
	if (!emu_state) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Failed to allocate emulator state");
		return NULL;
	}

	emu_state->screenTick = SDL_GetPerformanceFrequency() / 50;
	emu_state->screenThen = SDL_GetPerformanceCounter();

	return emu_state;
}

bool emulatorInteration(void *state)
{
	emulator_state_t *emu_state = (emulator_state_t *)state;
	bool irq = false;

	m68k_execute(50000);
	uint64_t now = SDL_GetPerformanceCounter();

	if ((now - emu_state->screenThen) > emu_state->screenTick) {
		emulatorUpdatePixelBuffer();
		emulatorRenderScreen();

		EMU_PC_INTR |= PC_INTRF;
		irq = true;

		emu_state->screenThen += emu_state->screenTick;
	}

	if (utarray_len(q68_kbd_queue)) {
		Q68_KBD_STATUS |= KBD_RCV;
		irq = true;
	}

	if (irq) {
		m68k_set_irq(2);
		irq = false;
	}
	return true;
}
