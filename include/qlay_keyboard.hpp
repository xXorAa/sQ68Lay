/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <queue>
#include <SDL.h>

namespace qlaykbd {

extern std::queue<int> keyBuffer;
extern int keysPressed;

extern SDL_SpinLock qlay_kbd_lock;

uint8_t getKeyrow(uint8_t row);
void processKey(int scancode, bool pressed);

} // namespace qlaykbd
