#include <stdbool.h>
#include <stdio.h>

#include "emulator_options.h"
#include "m68k.h"
#include "qlay_ipc.h"

unsigned int cycleNextEvent = 0;
bool trace = false;

void emu_hook_pc(unsigned int pc)
{
	if (trace) {
		char disBuf[256];
		uint32_t d[8];
		uint32_t a[8];
		uint32_t sr;

		int i;

		for (i = 0; i < 8; i++) {
			d[i] = m68k_get_reg(NULL, M68K_REG_D0 + i);
		}

		for (i = 0; i < 8; i++) {
			a[i] = m68k_get_reg(NULL, M68K_REG_A0 + i);
		}

		sr = m68k_get_reg(NULL, M68K_REG_SR) & 0x1F;

		m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);

		// Output the registers and assembly
		printf("  | %8c| %8c| %8c| %8c| %8c| %8c| %8c| %8c|\n", '0', '1', '2', '3', '4', '5', '6', '7');
		printf("D | %8x| %8x| %8x| %8x| %8x| %8x| %8x| %8x|\n", d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
		printf("A | %8x| %8x| %8x| %8x| %8x| %8x| %8x| %8x|\n", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
		printf("S | X=%1d N=%1d Z=%1d V=%1d C=%1d\n", (sr >> 4) & 1, (sr >> 3) & 1, (sr >> 2) & 1, (sr >> 1) & 1, sr & 1);
		printf("\n");	
		printf("%04X %s\n\n", pc, disBuf);
	}
}
