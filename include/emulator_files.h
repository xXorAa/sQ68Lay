/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef EMULATOR_FILES_H
#define EMULATOR_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool emulatorFileExists(const char* name);
size_t emulatorFileSize(const char* name);
bool emulatorLoadFile(const char* name, void* addr, size_t size);

#endif /* EMULATOR_FILES_H */
