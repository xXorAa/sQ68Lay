/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "emulator_keyboard.h"

bool emulatorProcessEvents(SDL_Event* event)
{
  switch (event->type) {
  case SDL_EVENT_QUIT:
    return false;
    break;
  case SDL_EVENT_KEY_DOWN:
    emulatorProcessKey(event->key.key, event->key.scancode, 1);
    break;
  case SDL_EVENT_KEY_UP:
    emulatorProcessKey(event->key.key, event->key.scancode, 0);
    break;
  default:
    break;
  }

  return true;
}
