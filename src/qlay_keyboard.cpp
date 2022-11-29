/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include <SDL.h>

#include "qlay_keyboard.hpp"

namespace qlaykbd {

SDL_SpinLock qlay_kbd_lock;

struct qlKey {
    int keycode;
    int flags;
};

std::map<int, qlKey> qlMapDefault = {
    // Cursor Keys
    { SDLK_LEFT, {49, 0 }},
    { SDLK_UP, {50, 0 }},
    { SDLK_RIGHT, {52, 0 }},
    { SDLK_DOWN, {55, 0 }},
    // F Keys
    { SDLK_F1, {57, 0 }},
    { SDLK_F2, {59, 0 }},
    { SDLK_F3, {60, 0 }},
    { SDLK_F4, {56, 0 }},
    { SDLK_F5, {61, 0 }},
    // Other Keys
    { SDLK_RETURN, {48, 0 }},
    { SDLK_SPACE, {54, 0 }},
    { SDLK_TAB, {19, 0 }},
    { SDLK_ESCAPE, {51, 0 }},
    { SDLK_CAPSLOCK, {33, 0 }},
    { SDLK_RIGHTBRACKET, {40, 0 }},
    { SDLK_5, {58, 0 }},
    { SDLK_4, {62, 0 }},
    { SDLK_7, {63, 0 }},
    { SDLK_LEFTBRACKET, {32, 0 }},
    { SDLK_z, {41, 0 }},
    { SDLK_PERIOD, {42, 0 }},
    { SDLK_c, {43, 0 }},
    { SDLK_b, {44, 0 }},
    { SDLK_BACKQUOTE, {45, 0 }},
    { SDLK_m, {46, 0 }},
    { SDLK_QUOTE, {47, 0 }},
    { SDLK_BACKSLASH, {53, 0 }},
    { SDLK_k, {34, 0 }},
    { SDLK_s, {35, 0 }},
    { SDLK_f, {36, 0 }},
    { SDLK_EQUALS, {37, 0 }},
    { SDLK_g, {38, 0 }},
    { SDLK_SEMICOLON, {39, 0 }},
    { SDLK_l, {24, 0 }},
    { SDLK_3, {25, 0 }},
    { SDLK_h, {26, 0 }},
    { SDLK_1, {27, 0 }},
    { SDLK_a, {28, 0 }},
    { SDLK_p, {29, 0 }},
    { SDLK_d, {30, 0 }},
    { SDLK_j, {31, 0 }},
    { SDLK_9, {16, 0 }},
    { SDLK_w, {17, 0 }},
    { SDLK_i, {18, 0 }},
    { SDLK_r, {20, 0 }},
    { SDLK_MINUS, {21, 0 }},
    { SDLK_y, {22, 0 }},
    { SDLK_o, {23, 0 }},
    { SDLK_8, {8, 0 }},
    { SDLK_2, {9, 0 }},
    { SDLK_6, {10, 0 }},
    { SDLK_q, {11, 0 }},
    { SDLK_e, {12, 0 }},
    { SDLK_0, {13, 0 }},
    { SDLK_t, {14, 0 }},
    { SDLK_u, {15, 0 }},
    { SDLK_x, {3, 0 }},
    { SDLK_v, {4, 0 }},
    { SDLK_SLASH, {5, 0 }},
    { SDLK_n, {6, 0 }},
    { SDLK_COMMA, {7, 0 }},
    { SDLK_LSHIFT, {0x400, 0}},
    { SDLK_RSHIFT, {0x400, 0}},
    { SDLK_LCTRL, {0x200, 0}},
    { SDLK_RCTRL, {0x200, 0}},
    { SDLK_LALT, {0x100, 0}},
    { SDLK_RALT, {0x100, 0}},
};

std::vector<bool> keyState(0x800);
int keysPressed;
std::queue<int> keyBuffer;

void processKey(int scancode, bool pressed)
{
    std::cout << "Scancode: " << std::hex << scancode << std::endl;

    auto keyi = qlMapDefault.find(scancode);

    // Exit here if no key found
    if (keyi == qlMapDefault.end()) {
        return;
    }

    auto qlKey = keyi->second.keycode;

    std::cout << "QLKey " << std::hex << qlKey << std::endl;

    // Key is already pressed
    if (keyState[qlKey] == pressed) {
        return;
    }

    // Store the key
    keyState[qlKey] = pressed;

    // Count the number of normal keys pressed
    if (qlKey & 0x7f) {
        if (pressed) {
            keysPressed++;
        } else {
            keysPressed--;
        }
    }

    // Handle Shift, Ctrl, Alt
    keyState[2]=keyState[0x100]|keyState[0x180];
    keyState[1]=keyState[0x200];
    keyState[0]=keyState[0x400];

    if (pressed) {
        if (qlKey<0x80) {
            if (keyState[0x180]) qlKey+=0x180;
            if (keyState[0x100]) qlKey+=0x100;
            if (keyState[0x200]) qlKey+=0x200;
            if (keyState[0x400]) qlKey+=0x400;
            SDL_AtomicLock(&qlay_kbd_lock);
            keyBuffer.push(qlKey);
            SDL_AtomicUnlock(&qlay_kbd_lock);
        } else {
            if ((qlKey & 0x780) != qlKey) {
                SDL_AtomicLock(&qlay_kbd_lock);
                keyBuffer.push(qlKey); // BS, DEL
                SDL_AtomicUnlock(&qlay_kbd_lock);
            }
        }
    }
}

} // namespace qlaykbd
