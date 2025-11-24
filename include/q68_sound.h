#pragma once

#ifndef Q68_SOUND_H
#define Q68_SOUND_H

#include <SDL3/SDL.h>

bool q68InitSound(void);
void q68PlayByte(int channel, Uint8 byte);

#endif // Q68_SOUND_H
