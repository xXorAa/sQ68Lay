#pragma once

#ifndef EMULATOR_MAINLOOP_H
#define EMULATOR_MAINLOOP_H

#include <stdbool.h>

int emulatorMainLoop(void);

extern bool doIrq;

#endif /* EMULATOR_MAINLOOP_H */
