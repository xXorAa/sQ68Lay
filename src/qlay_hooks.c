#include <stdbool.h>
#include <stdio.h>

#include "m68k.h"

unsigned int cyclesThen = 0;
unsigned int cycleNextEvent = 0;
bool doIrq;

unsigned int cycles()
{
    return cyclesThen + m68k_cycles_run();
}

void emu_hook_pc(unsigned int pc)
{
	if (cycles() >= cycleNextEvent) {
		//ipc_do_next_event();
	}

	if (doIrq) {
		m68k_set_irq(2);
		doIrq = false;
	}

	char disBuf[256];
	m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);
	printf("%04X %s\n", pc, disBuf);
}
