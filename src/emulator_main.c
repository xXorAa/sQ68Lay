/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>

#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"

#if __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

int main(int argc, char **argv)
{
	emulatorOptionParse(argc, argv);

#if __EMSCRIPTEN__
	int ret = SDL_Init(SDL_INIT_EVERYTHING &
			   ~(SDL_INIT_TIMER | SDL_INIT_HAPTIC));
#else
	int ret = SDL_Init(SDL_INIT_EVERYTHING);
#endif
	if (ret < 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return ret;
	}

	emulatorInitMemory();
	emulatorScreenInit(1);

#if __EMSCRIPTEN__
	emscripten_set_main_loop(emulatorMainLoop, -1, 1);
	return 0;
#else
	emulatorMainLoop();
	return 0;
#endif
}
