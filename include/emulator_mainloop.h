#pragma once

#ifndef EMULATOR_MAINLOOP_H
#define EMULATOR_MAINLOOP_H

#include <stdbool.h>
#include <stdint.h>

uint64_t cycles(void);
int emulatorMainLoop(void);

extern unsigned int extraCycles;
extern uint64_t cyclesNow;
extern bool doIrq;

#endif /* EMULATOR_MAINLOOP_H */
