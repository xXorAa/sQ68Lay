/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>

#include "emulator_events.h"
#include "emulator_files.h"
#include "emulator_hardware.h"
#include "emulator_keyboard.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "m68k.h"
#include "q68_hooks.h"
#include "q68_keyboard.h"
#include "q68_sd.h"
#include "utarray.h"

uint32_t msClk = 0;
uint32_t msClkNextEvent = 0;

void emulatorMainLoop(void)
{
	const char *smsqe = emulatorOptionString("smsqe");
	const char *sysrom = emulatorOptionString("sysrom");

	uint32_t initPc = 0;

	if (strlen(smsqe) > 0) {
		emulatorLoadFile(smsqe, &emulatorMemorySpace()[Q68_SMSQE_ADDR], 0);
		initPc = Q68_SMSQE_ADDR;
	} else if (strlen(sysrom) > 0) {
		emulatorLoadFile(sysrom, &emulatorMemorySpace()[Q68_SYSROM_ADDR], 0);
		romProtect = true;
	}

	if (emulatorOptionInt("trace")) {
		trace = true;
	}

	q68InitKeyb();
	q68InitSD();

	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_init();
	m68k_pulse_reset();
	if (initPc) {
		m68k_set_reg(M68K_REG_PC, initPc);
	}

	uint64_t counterFreq = SDL_GetPerformanceFrequency();
	uint64_t screenTick = counterFreq / 50;

	uint64_t screenThen = SDL_GetPerformanceCounter();

	while (!exitLoop) {
		bool irq = false;

		m68k_execute(50000);
		uint64_t now = SDL_GetPerformanceCounter();

		if ((now - screenThen) > screenTick) {
                        emulatorUpdatePixelBuffer();
                        emulatorRenderScreen();
                        emulatorProcessEvents();

			EMU_PC_INTR |= PC_INTRF;
			irq = true;

			screenThen += screenTick;
		}

		if (utarray_len(q68_kbd_queue)) {
			Q68_KBD_STATUS |= KBD_RCV;
			irq = true;
		}

		if (irq) {
			m68k_set_irq(2);
			irq = false;
		}
	}
}
