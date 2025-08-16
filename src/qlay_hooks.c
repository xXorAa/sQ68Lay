#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "emulator_options.h"
#include "emulator_trace.h"
#include "m68k.h"

void emu_hook_pc(unsigned int pc)
{
	(void)pc; // Suppress unused parameter warning
	emulatorTrace();
}
