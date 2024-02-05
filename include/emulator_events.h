/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef EMULATOR_EVENTS_H
#define EMULATOR_EVENTS_H

#include <stdbool.h>

extern bool exitLoop;

void emulatorProcessEvents(void);

#endif /* EMULATOR_EVENTS_H */
