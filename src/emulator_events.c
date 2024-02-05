/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>
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
	case SDL_QUIT:
		exitLoop = 1;
		break;
	case SDL_KEYDOWN:
		emulatorProcessKey(event.key.keysym.sym, event.key.keysym.scancode, 1);
		break;
	case SDL_KEYUP:
		emulatorProcessKey(event.key.keysym.sym, event.key.keysym.scancode, 0);
		break;
	default:
		break;
	}
}
