#include <stdbool.h>
#include <stdio.h>

#include "emulator_options.h"
#include "m68k.h"
#include "qlay_ipc.h"

unsigned int cycleNextEvent = 0;
bool trace = false;

unsigned int cycles(void)
{
//    return cyclesThen + m68k_cycles_run();
	return 0;
}

void emu_hook_pc(unsigned int pc)
{
	if (trace) {
		char disBuf[256];
		m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);
		printf("%04X %s\n", pc, disBuf);
	}
}
