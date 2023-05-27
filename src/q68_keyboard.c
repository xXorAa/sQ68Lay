/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <glib.h>
#include <SDL.h>

#include "emulator_events.h"
#include "emulator_hardware.h"
#include "emulator_keyboard.h"
#include "q68_hooks.h"
#include "sdl-ps2.h"

// keyboard lock and queue
GQueue* q68_kbd_queue;

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
		g_queue_push_tail(q68_kbd_queue, GINT_TO_POINTER(queue[i]));
	}
}
