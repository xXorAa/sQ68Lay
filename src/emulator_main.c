/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <stdio.h>

#include "emulator_events.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "emulator_screen.h"

#if __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	emulatorOptionParse(argc, argv);

	SDL_SetAppMetadata(EMU_STR " emulator for the Sinclair QL", "0.1",
			   "https://github.com/xXorAa/sQ68Lay/");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init Error: %s",
			     SDL_GetError());
		return SDL_APP_FAILURE;
	}

#ifdef QLAY_EMU
	// limit frame rate to 50Hz for qlay3
	SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "50");
#endif

	// BUG: workaround https://github.com/libsdl-org/SDL/issues/12805
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	SDL_Log("Directory: %s", SDL_GetCurrentDirectory());

	emulatorInitMemory();
	emulatorInitScreen(1);

	*appstate = emulatorInitEmulation();
	if (!*appstate) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Failed Emulation Init");
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	(void)appstate;

	emulatorInteration(appstate);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	(void)appstate;
	if (emulatorProcessEvents(event)) {
		return SDL_APP_CONTINUE;
	}
	return SDL_APP_SUCCESS;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	(void)appstate;
	(void)result;

	SDL_Quit();
}
