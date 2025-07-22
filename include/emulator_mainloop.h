#pragma once

#ifndef EMULATOR_MAINLOOP_H
#define EMULATOR_MAINLOOP_H

#include <stdbool.h>
#include <stdint.h>

uint64_t cycles(void);
void *emulatorInitEmulation(void);
bool emulatorInteration(void *state);

extern unsigned int extraCycles;

#endif /* EMULATOR_MAINLOOP_H */
