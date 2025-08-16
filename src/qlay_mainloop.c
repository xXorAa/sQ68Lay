/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
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
#include "qlay_sound.h"

#define MDV_CYCLES 230
#define FIFTYHZ_CYCLES 150000
#define POINTONEMS_CYCLES 750

typedef struct {
	uint64_t cyclesNow;
	uint64_t cyclesThen;
	uint64_t cyclesMdv;
	uint64_t frameCount;
} emulator_state_t;

int msClk = 0;

unsigned int extraCycles;

uint64_t cyclesNow = 0;
bool doIrq = false;

uint64_t cycles(void)
{
	return cyclesNow / 16;
}

void *emulatorInitEmulation(void)
{
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_init();
	m68k_pulse_reset();

	qlayInitSound();
	qlayInitIPCSound();
	qlayInitAYSound();
	qlayInitKbd();
	qlayInitIPC();
	qlayInitDisk();
	qlayInitialiseTime();
	qlayInitialiseQsound();

	emulator_state_t *emu_state = calloc(1, sizeof(emulator_state_t));
	return emu_state;
}

bool emulatorInteration(void *state)
{
	emulator_state_t *emu_state = (emulator_state_t *)state;

	while ((emu_state->cyclesNow - emu_state->cyclesThen) <
	       FIFTYHZ_CYCLES) {
		extraCycles = 0;
		emu_state->cyclesNow += m68k_execute(1) + extraCycles;

		if ((emu_state->cyclesNow - emu_state->cyclesMdv) >
		    MDV_CYCLES) {
			do_mdv_tick();

			emu_state->cyclesMdv += MDV_CYCLES;
		}
	}

	emulatorUpdatePixelBuffer();
	emulatorRenderScreen();

	EMU_PC_INTR |= PC_INTRF;

	m68k_set_irq(2);

	emu_state->cyclesThen += FIFTYHZ_CYCLES;

	// update the RTC register
	emu_state->frameCount++;
	if (emu_state->frameCount % 50 == 0) {
		EMU_PC_CLOCK++;
	}

	return 0;
}
