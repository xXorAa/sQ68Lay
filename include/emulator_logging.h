/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */
#pragma once

#ifndef EMULATOR_LOGGING
#define EMULATOR_LOGGING

#include <SDL3/SDL.h>

enum {
	QLAY_LOG_SOUND = SDL_LOG_CATEGORY_CUSTOM,
	QLAY_LOG_IPC,
	QLAY_LOG_MDV,
	QLAY_LOG_IO,
	QLAY_LOG_HW,
};

enum {
	Q68_LOG_SOUND = SDL_LOG_CATEGORY_CUSTOM,
	Q68_LOG_SD,
	Q68_LOG_HW,
	Q68_LOG_DISK,
};

#endif //EMULATOR_LOGGIN
