/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <queue>
#include <SDL.h>
#include <vector>

#include "q68_hardware.hpp"
#include "sdl-ps2.h"

namespace emulator {

bool exitLoop = false;

static void q68ProcessKey(SDL_Keysym *keysym, int pressed)
{
    int qlen = 0;

    std::vector<uint8_t> queue(MAX_PS2_CODE_LEN);

    qlen = ps2_encode(keysym->scancode, pressed, queue.data());

    SDL_AtomicLock(&q68_kbd_lock);
    for (int i = 0; i < qlen; i++) {
        q68_kbd_queue.push(queue[i]);
    }
    SDL_AtomicUnlock(&q68_kbd_lock);

}

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
    case SDL_KEYDOWN:
        q68ProcessKey(&event.key.keysym, 1);
        break;
    case SDL_KEYUP:
        q68ProcessKey(&event.key.keysym, 0);
        break;
    default:
        break;
    }   
}

} //namespace emulator