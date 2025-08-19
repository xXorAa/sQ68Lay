#pragma once
#ifndef Q68_DISK_H
#define Q68_DISK_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool q68DiskInitialise(void);
Uint32 q68DiskReadSMSQE(void);

#endif // Q68_DISK_H
