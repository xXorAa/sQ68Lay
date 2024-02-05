
/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#pragma once

#ifndef QLAY_TRACE_H
#define QLAY_TRACE_H

#include <stdbool.h>

void traceInit(void);
bool traceEnabled(void);
const char *traceSymbol(int addr);

#endif /* QLAY_TRACE_H */
