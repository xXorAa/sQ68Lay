#pragma once

#ifndef EMULATOR_MEMORY_H
#define EMULATOR_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

uint8_t *emulatorMemorySpace(void);
uint8_t *emulatorScreenSpace(void);
int emulatorInitMemory(void);
extern bool romProtect;

#define KB(x) ((size_t)(x) << 10)
#define MB(x) ((size_t)(x) << 20)

#define Q68_ROM_SIZE KB(96)
#define Q68_RAM_SIZE MB(28)

#define QL_INTERNAL_IO 0x18000
#define QL_INTERNAL_IO_SIZE 0x100

#define QLAY_NFA_IO 0x18100
#define QLAY_NFA_IO_SIZE 0x300

#define QL_EXTERNAL_IO 0x1C000
#define QL_EXTERNAL_IO_SIZE KB(16)

#define Q68_SCREEN 0xFE800000
#define Q68_SCREEN_SIZE MB(4)

#define Q68_Q40_IO 0xFF000000
#define Q68_Q40_IO_SIZE MB(16)

#define Q68_SYSROM_ADDR 0x0
#define Q68_SMSQE_WIN_ADDR 0x320000
#define Q68_SMSQE_WIN_OFFSIZE 0x1040
#define Q68_SMSQE_RAM_ADDR 0x300000
#define Q68_INVALID 0xFFFFFFFF

#endif /* EMULATOR_MEMORY_H */
