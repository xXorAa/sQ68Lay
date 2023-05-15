/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <glib.h>
#include <SDL.h>

#include "qlay_hooks.h"
#include "qlay_keyboard.h"
#include "qlkeys.h"

struct qlKey {
	int keycode;
	int flags;
};

struct qlayKeyEntry {
	int sdlKey;
	struct qlKey key;
};

const struct qlayKeyEntry qlMapDefault[] = {
	// Cursor Keys
	{ SDLK_LEFT, { QL_LEFT, 0 } },
	{ SDLK_UP, { QL_UP, 0 } },
	{ SDLK_RIGHT, { QL_RIGHT, 0 } },
	{ SDLK_DOWN, { QL_DOWN, 0 } },
	// F Keys
	{ SDLK_F1, { QL_F1, 0 } },
	{ SDLK_F2, { QL_F2, 0 } },
	{ SDLK_F3, { QL_F3, 0 } },
	{ SDLK_F4, { QL_F4, 0 } },
	{ SDLK_F5, { QL_F5, 0 } },
	// Other Keys
	{ SDLK_RETURN, { QL_ENTER, 0 } },
	{ SDLK_SPACE, { QL_SPACE, 0 } },
	{ SDLK_TAB, { QL_TAB, 0 } },
	{ SDLK_ESCAPE, { QL_ESCAPE, 0 } },
	{ SDLK_CAPSLOCK, { QL_CAPSLOCK, 0 } },
	{ SDLK_RIGHTBRACKET, { QL_RBRACKET, 0 } },
	{ SDLK_LEFTBRACKET, { QL_LBRACKET, 0 } },
	{ SDLK_PERIOD, { QL_PERIOD, 0 } },
	{ SDLK_BACKQUOTE, { QL_POUND, 0 } },
	{ SDLK_QUOTE, { QL_QUOTE, 0 } },
	{ SDLK_BACKSLASH, { QL_BACKSLASH, 0 } },
	{ SDLK_EQUALS, { QL_EQUAL, 0 } },
	{ SDLK_SEMICOLON, { QL_SEMICOLON, 0 } },
	{ SDLK_MINUS, { QL_MINUS, 0 } },
	{ SDLK_SLASH, { QL_SLASH, 0 } },
	{ SDLK_COMMA, { QL_COMMA, 0 } },
	// Numbers
	{ SDLK_0, { QL_0, 0 } },
	{ SDLK_1, { QL_1, 0 } },
	{ SDLK_2, { QL_2, 0 } },
	{ SDLK_3, { QL_3, 0 } },
	{ SDLK_4, { QL_4, 0 } },
	{ SDLK_5, { QL_5, 0 } },
	{ SDLK_6, { QL_6, 0 } },
	{ SDLK_7, { QL_7, 0 } },
	{ SDLK_8, { QL_8, 0 } },
	{ SDLK_9, { QL_9, 0 } },
	// Letters
	{ SDLK_a, { QL_A, 0 } },
	{ SDLK_b, { QL_B, 0 } },
	{ SDLK_c, { QL_C, 0 } },
	{ SDLK_d, { QL_D, 0 } },
	{ SDLK_e, { QL_E, 0 } },
	{ SDLK_f, { QL_F, 0 } },
	{ SDLK_g, { QL_G, 0 } },
	{ SDLK_h, { QL_H, 0 } },
	{ SDLK_i, { QL_I, 0 } },
	{ SDLK_j, { QL_J, 0 } },
	{ SDLK_k, { QL_K, 0 } },
	{ SDLK_l, { QL_L, 0 } },
	{ SDLK_m, { QL_M, 0 } },
	{ SDLK_n, { QL_N, 0 } },
	{ SDLK_o, { QL_O, 0 } },
	{ SDLK_p, { QL_P, 0 } },
	{ SDLK_q, { QL_Q, 0 } },
	{ SDLK_r, { QL_R, 0 } },
	{ SDLK_s, { QL_S, 0 } },
	{ SDLK_t, { QL_T, 0 } },
	{ SDLK_u, { QL_U, 0 } },
	{ SDLK_v, { QL_V, 0 } },
	{ SDLK_w, { QL_W, 0 } },
	{ SDLK_x, { QL_X, 0 } },
	{ SDLK_y, { QL_Y, 0 } },
	{ SDLK_z, { QL_Z, 0 } },
	// modifier keys
	{ SDLK_LSHIFT, { QL_SHIFT, 0 } },
	{ SDLK_RSHIFT, { QL_SHIFT, 0 } },
	{ SDLK_LCTRL, { QL_CTRL, 0 } },
	{ SDLK_RCTRL, { QL_CTRL, 0 } },
	{ SDLK_LALT, { QL_ALT, 0 } },
	{ SDLK_RALT, { QL_ALT, 0 } },
	// composed keys
	{ SDLK_BACKSPACE, { QL_CTRL | QL_LEFT, 0 } },
	{ SDLK_DELETE, { QL_CTRL | QL_RIGHT, 0 } },
	{ 0x00, {0x00, 0x00 } },
};

static int keyState[0x800];
int qlayKeysPressed;
GQueue *qlayKeyBuffer;

void qlayInitKbd(void)
{
	qlayKeyBuffer = g_queue_new();
}

uint8_t qlayGetKeyrow(uint8_t row)
{
	row &= 7;
	row = 7 - row;
	row <<= 3;

	uint8_t rv = 0;
	for (int b = 7; b > -1; b--) {
		rv <<= 1;
		if (keyState[row + b]) {
			rv++;
		}
	}
	return rv;
}

void emulatorProcessKey(int keysym, __attribute__ ((unused)) int scancode, bool pressed)
{
	int qlKey;
	int i = 0;

	if ((keysym == SDLK_F12) && pressed) {
		if (!trace) {
			trace = true;
		} else {
			trace = false;
		}
	}
	while (qlMapDefault[i].sdlKey != 0) {
		if (qlMapDefault[i].sdlKey == keysym) {
			break;
		}

		i++;
	}

	// Exit here if no key found
	if (qlMapDefault[i].sdlKey == 0) {
		return;
	}

	qlKey = qlMapDefault[i].key.keycode;

	// Key is already pressed
	if (keyState[qlKey] == pressed) {
		return;
	}

	// Store the key
	keyState[qlKey] = pressed;

	// Count the number of normal keys pressed
	if (qlKey & 0x7f) {
		if (pressed) {
			qlayKeysPressed++;
		} else {
			qlayKeysPressed--;
		}
	}

	// Handle Shift, Ctrl, Alt
	keyState[2] = keyState[0x100] | keyState[0x180];
	keyState[1] = keyState[0x200];
	keyState[0] = keyState[0x400];

	if (pressed) {
		if (qlKey < 0x80) {
			if (keyState[0x180])
				qlKey += 0x180;
			if (keyState[0x100])
				qlKey += 0x100;
			if (keyState[0x200])
				qlKey += 0x200;
			if (keyState[0x400])
				qlKey += 0x400;
			g_queue_push_tail(qlayKeyBuffer,
					  GINT_TO_POINTER(qlKey));
			printf("queuesize %d\n", g_queue_get_length(qlayKeyBuffer));
		} else {
			if ((qlKey & 0x780) != qlKey) {
				g_queue_push_tail(
					qlayKeyBuffer,
					GINT_TO_POINTER(qlKey)); // BS, DEL
			}
		}
	}
}
