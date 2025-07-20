/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "emulator_hardware.h"
#include "emulator_keyboard.h"
#include "sdl-ps2.h"

bool exitLoop = false;

void emulatorProcessEvents(void)
{
	SDL_Event event;

	if (!SDL_PollEvent(&event)) {
		return;
	}

	switch (event.type) {
	case SDL_EVENT_QUIT:
		exitLoop = 1;
		break;
	case SDL_EVENT_KEY_DOWN:
		emulatorProcessKey(event.key.key, event.key.scancode, 1);
		break;
	case SDL_EVENT_KEY_UP:
		emulatorProcessKey(event.key.key, event.key.scancode, 0);
		break;
	default:
		break;
	}
}
