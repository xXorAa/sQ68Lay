/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

namespace emulator {

extern bool q68RenderScreenFlag;

void q68ScreenInit(void);
void q68UpdatePixelBuffer(uint8_t *q68ScreenPtr, int length, int mode);
void q68RenderScreen(void);

}