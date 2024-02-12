/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>
#include <stdint.h>

#include "emulator_events.h"
#include "emulator_files.h"
#include "qlay_hooks.h"
#include "emulator_hardware.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "m68k.h"
#include "qlay_disk.h"
#include "qlay_hooks.h"
#include "qlay_io.h"
#include "qlay_keyboard.h"
#include "qlay_trace.h"

int msClk = 0;

unsigned int extraCycles;

uint64_t cyclesNow = 0;
bool doIrq = false;

uint64_t cycles(void)
{
	return cyclesNow / 16;
}

int emulatorMainLoop(void)
{
	int expromCount;

	emulatorLoadFile(emulatorOptionString("sysrom"),
			 &emulatorMemorySpace()[0], 0);

	// TODO: add proper exprom handling
	expromCount = emulatorOptionDevCount("exprom");
	if (expromCount) {
		emulatorLoadFile(emulatorOptionDev("exprom", 0),
				 &emulatorMemorySpace()[KB(48)], 0);
	}

	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_init();
	m68k_pulse_reset();

	qlayInitKbd();
	qlayInitIPC();
	qlayInitDisk();
	traceInit();

	uint64_t counterFreq = SDL_GetPerformanceFrequency();
	uint64_t msTick = counterFreq / 10000;

	uint64_t screenThen = SDL_GetPerformanceCounter();
	uint64_t msThen = screenThen;

	uint64_t cyclesThen = 0;

	uint64_t cycles50hz = 150000;
	uint64_t cyclesMdv = 300;

	//do_next_event();

	while (!exitLoop) {
		while ((cyclesNow - cyclesThen) < 750) {
			extraCycles = 0;
			cyclesNow += m68k_execute(1) + extraCycles;

			if (cycles() >= cycleNextEvent) {
				//do_next_event();
			}

			if (cyclesNow >= cycles50hz) {
				emulatorUpdatePixelBuffer();
				emulatorRenderScreen();
				emulatorProcessEvents();

				EMU_PC_INTR |= PC_INTRF;
				doIrq = true;

				cycles50hz += 150000;
			}

			if (cyclesNow >= cyclesMdv) {
				do_mdv_tick();

				cyclesMdv += 300;
			}
		}

		if (doIrq) {
			m68k_set_irq(2);
			doIrq = false;
		}

		cyclesThen = cyclesNow;

		/* wait for next 0.1ms */
		uint64_t now = SDL_GetPerformanceCounter();
		while ((now - msThen) < msTick) {
			now = SDL_GetPerformanceCounter();
		}

		msThen = now;
	}

	return 0;
}
