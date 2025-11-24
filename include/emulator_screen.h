#pragma once

#ifndef EMULATOR_SCREEN_H
#define EMULATOR_SCREEN_H

#include <stdbool.h>

#ifdef QLAY_EMU
#define EMULATOR_FRAMERATE_NORMAL "50"
#define EMULATOR_FRAMERATE_FAST "400"
#else
#define EMULATOR_FRAMERATE_NORMAL "0"
#define EMULATOR_FRAMERATE_FAST "0"
#endif

int emulatorInitScreen(int screenMode);
void emulatorUpdatePixelBuffer(void);
void emulatorRenderScreen(void);
void emulatorScreenChangeMode(int qlMode);
void emulatorToggleFullScreen(void);
void emulatorSetRefresh(bool fast);
void emulatorToggleRefresh(void);

extern bool emulatorSecondScreen;

#endif /* EMULATOR_SCREEN_H */
