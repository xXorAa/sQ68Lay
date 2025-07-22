#pragma once

#ifndef EMULATOR_SCREEN_H
#define EMULATOR_SCREEN_H

#include <stdbool.h>

int emulatorInitScreen(int screenMode);
void emulatorUpdatePixelBuffer(void);
void emulatorRenderScreen(void);
void emulatorScreenChangeMode(int qlMode);

extern bool emulatorSecondScreen;

#endif /* EMULATOR_SCREEN_H */
