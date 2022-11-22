/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <SDL.h>

namespace emulator {

bool exitLoop = false;

void q68ProcessEvents(void)
{
    SDL_Event event;

    if (!SDL_PollEvent(&event)) {
        return;
    }

    switch(event.type) {
        case SDL_QUIT:
            exitLoop = 1;
            break;
        default:
            break;
    }   
}

} //namespace emulator