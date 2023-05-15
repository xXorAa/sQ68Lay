#pragma once

#ifndef EMULATOR_MAINLOOP_H
#define EMULATOR_MAINLOOP_H

#include <stdbool.h>

int emulatorMainLoop(void);

extern unsigned int extraCycles;

#endif /* EMULATOR_MAINLOOP_H */
