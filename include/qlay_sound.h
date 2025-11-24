/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once
#include <SDL3/SDL.h>

#ifndef QLAY_SOUND_H
#define QLAY_SOUND_H

bool qlayInitSound(void);
bool qlayInitMdvSound(void);
bool qlayStartMdvSound(void);
bool qlayStopMdvSound(void);
bool qlayInitIPCSound(void);
bool qlayInitAYSound(void);
void qlaySetAYRegister(Uint8 regNum, Uint8 regVal);

void qlayIPCBeepSound(Uint8* buffer);
void qlayIPCKillSound(void);

#endif /* QLAY_SOUND_H */
