/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef QLAY_SOUND_H
#define QLAY_SOUND_H

bool qlayInitSound(void);
bool qlayInitMdvSound(void);
bool qlayStartMdvSound(void);
bool qlayStopMdvSound(void);
bool qlayInitIPCSound(void);

void qlayIPCBeepSound(Uint8 *buffer);
void qlayIPCKillSound(void);

#endif /* QLAY_SOUND_H */
