#pragma once

#ifndef QLAY_HOOKS_H
#define QLAY_HOOKS_H

unsigned int cycles();

extern unsigned int cycleNextEvent;
extern unsigned int cyclesThen;
extern bool doIrq;

#endif /* QLAY_HOOKS_H */
