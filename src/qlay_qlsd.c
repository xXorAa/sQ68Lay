/*
 * Copyright (c) 2025 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>

#include "emulator_logging.h"
#include "emulator_options.h"
#include "spi_sdcard.h"

SDL_IOStream* sd1Stream = NULL;
SDL_IOStream* sd2Stream = NULL;
bool QLSDEnabled = false;

bool qlayQLSDInitialise(void)
{
  if (emulatorOptionInt("qlsd")) {
    QLSDEnabled = true;
  }

  const char* diskName = emulatorOptionString("sd1");
  if (!diskName || (strlen(diskName) == 0)) {
    SDL_LogDebug(QLAY_LOG_DISK, "No SD card specified for SD1");
  } else {
    sd1Stream = SDL_IOFromFile(diskName, "r+b");
    if (sd1Stream == NULL) {
      SDL_LogError(QLAY_LOG_DISK,
          "Failed to open SD1 image: %s %s",
          diskName, SDL_GetError());
      return false;
    }
  }

  diskName = emulatorOptionString("sd2");
  if (!diskName || (strlen(diskName) == 0)) {
    SDL_LogDebug(QLAY_LOG_DISK, "No SD card specified for SD2");
  } else {
    sd2Stream = SDL_IOFromFile(diskName, "r+b");
    if (sd2Stream == NULL) {
      SDL_LogError(QLAY_LOG_DISK,
          "Failed to open SD2 card: %s %s", diskName,
          SDL_GetError());
      return false;
    }
  }

  card_initialise(sd1Stream, sd2Stream);

  return true;
}
