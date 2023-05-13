/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>

#include "emulator_events.h"
#include "emulator_files.h"
#include "qlay_hooks.h"
#include "emulator_hardware.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "m68k.h"
#include "qlay_hooks.h"
#include "qlay_ipc.h"
#include "qlay_keyboard.h"

int msClk = 0;

int emulatorMainLoop(void)
{
	char *exprom;
	int expromCount;

	emulatorLoadFile(emulatorOptionString("sysrom"), &emulatorMemorySpace()[0], 0);

	expromCount = emulatorOptionDevCount("exprom");
	if (expromCount) {
		emulatorLoadFile(emulatorOptionDev("exprom", 0), &emulatorMemorySpace()[KB(48)], 0);
	}

	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_init();
	m68k_pulse_reset();

	qlayInitKbd();
	qlayInitIPC();
	//qlayInitDisk();

	uint64_t counterFreq = SDL_GetPerformanceFrequency();
	uint64_t screenTick = counterFreq / 50;
	uint64_t msTick = counterFreq / 10000;

	uint64_t screenThen = SDL_GetPerformanceCounter();
	uint64_t msThen = screenThen;

	int ql_loops = 0;

	while (!exitLoop) {
		uint64_t now = SDL_GetPerformanceCounter();

		if ((now - msThen) > msTick) {
			msThen = (msThen + msTick);
			msClk++;
			m68k_execute(750);
			cyclesThen += 750;
		}

		if ((now - screenThen) > screenTick){
			emulatorUpdatePixelBuffer();
			emulatorRenderScreen();
			emulatorProcessEvents();

			EMU_PC_INTR |= PC_INTRF;
			doIrq = true;

			screenThen += screenTick;
		}
	}

	return 0;
}
