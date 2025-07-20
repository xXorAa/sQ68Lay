/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef QLAY_KEYBOARD_H
#define QLAY_KEYBOARD_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <utarray.h>

extern int qlayKeysPressed;
extern UT_array *qlayKeyBuffer;

void qlayInitKbd(void);
uint8_t qlayGetKeyrow(uint8_t row);

#endif /* QLAY_KEYBOARD_H */
