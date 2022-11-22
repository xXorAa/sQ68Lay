/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>

#include "q68_emulator.hpp"
#include "q68_events.hpp"
#include "q68_memory.hpp"
#include "q68_screen.hpp"

int main(int argc, char **argv)
{
    int ret = SDL_Init(SDL_INIT_EVERYTHING); 
    if ( ret < 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return ret;
	}

    emulator::q68AllocateMemory();
    emulator::q68ScreenInit();

    SDL_Thread *thread = SDL_CreateThread(emulator::q68MainLoop, "sQ68ux Emu Thread", NULL);

    while(1) {
        if (emulator::q68RenderScreen) {
            emulator::q68UpdatePixelBuffer(emulator::q68MemorySpace + 0x20000, 0x8000, 4);
            emulator::q68RenderScreen();
            emulator::q68RenderScreenFlag = false;
        }
        emulator::q68ProcessEvents();

        if (emulator::exitLoop) {
            SDL_WaitThread(thread, NULL);
            return 0;
        }
    }
}