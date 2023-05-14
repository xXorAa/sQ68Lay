#pragma once

#ifndef QLAY_HOOKS_H
#define QLAY_HOOKS_H

#include <stdbool.h>

unsigned int cycles();

extern unsigned int cycleNextEvent;
extern unsigned int cyclesThen;
extern bool doIrq;
extern bool trace;

#endif /* QLAY_HOOKS_H */
