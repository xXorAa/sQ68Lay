#include <stdbool.h>
#include <stdio.h>

#include "emulator_options.h"
#include "m68k.h"

bool trace = false;

void emu_hook_pc(unsigned int pc)
{
	if (trace) {
		char disBuf[256];
		m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);
		printf("%08X %s\n", pc, disBuf);
	}
}
