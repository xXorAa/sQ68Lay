#include <stdbool.h>
#include <stdio.h>

#include "emulator_options.h"
#include "m68k.h"
#include "qlay_ipc.h"

unsigned int cyclesThen = 0;
unsigned int cycleNextEvent = 0;
bool doIrq = false;
bool trace = false;

unsigned int cycles()
{
    return cyclesThen + m68k_cycles_run();
}

void emu_hook_pc(unsigned int pc)
{
	if (cycles() >= cycleNextEvent) {
		do_next_event();
	}

	if (doIrq) {
		m68k_set_irq(2);
		doIrq = false;
	}

	if (trace) {
		char disBuf[256];
		m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);
		printf("%04X %s\n", pc, disBuf);
	}
}
