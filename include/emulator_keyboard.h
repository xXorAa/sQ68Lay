#pragma once

#ifndef EMULATOR_KEYBOARD_H
#define EMULATOR_KEYBOARD_H

#include <stdbool.h>

void emulatorProcessKey(int keysym, int scancode, bool pressed);

#endif /* EMULATOR_KEYBOARD_H */
