/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef EMULATOR_FILES_H
#define EMULATOR_FILES_H

#include <stdint.h>

int emulatorLoadFile(const char* name, void* addr, size_t size);

#endif /* EMULATOR_FILES_H */
