#pragma once

#ifndef EMULATOR_SCREEN_H
#define EMULATOR_SCREEN_H

void emulatorScreenInit(int screenMode);
void emulatorUpdatePixelBuffer(void);
void emulatorRenderScreen(void);
void emulatorScreenChangeMode(int qlMode);

#endif /* EMULATOR_SCREEN_H */
