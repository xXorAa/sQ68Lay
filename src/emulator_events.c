/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <stdbool.h>

#include "emulator_keyboard.h"
#include "emulator_screen.h"
#include "sdl-ps2.h"

static bool shift = false;

bool emulatorProcessEvents(SDL_Event* event)
{
  switch (event->type) {
  case SDL_EVENT_QUIT:
    return false;
    break;
  case SDL_EVENT_KEY_DOWN:
    switch (event->key.key) {
    case SDLK_LSHIFT:
      shift = true;
      break;
    case SDLK_F11:
      if (shift) {
        emulatorToggleRefresh();
      } else {
        emulatorToggleFullScreen();
      }
      break;
    };
    emulatorProcessKey(event->key.key, event->key.scancode, 1);
    break;
  case SDL_EVENT_KEY_UP:
    switch (event->key.key) {
    case SDLK_LSHIFT:
      shift = false;
      break;
    }
    emulatorProcessKey(event->key.key, event->key.scancode, 0);
    break;
  default:
    break;
  }

  return true;
}
