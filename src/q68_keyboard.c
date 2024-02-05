/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include "uthash/src/utarray.h"
#include <SDL.h>

#include "emulator_events.h"
#include "emulator_hardware.h"
#include "emulator_keyboard.h"
#include "q68_hooks.h"
#include "sdl-ps2.h"
#include "utarray.h"

// keyboard lock and queue
UT_array *q68_kbd_queue;

void q68InitKeyb(void)
{
	utarray_new(q68_kbd_queue, &ut_int_icd);
}

void emulatorProcessKey(int keysym, int scancode, bool pressed)
{
	int qlen = 0;
	uint8_t queue[MAX_PS2_CODE_LEN];

	if ((keysym == SDLK_F12) && pressed) {
		trace ^= 1;
		return;
	}

	qlen = ps2_encode(scancode, pressed, queue);

	for (int i = 0; i < qlen; i++) {
		int key = queue[i];
		utarray_push_back(q68_kbd_queue, &key);
	}
}
