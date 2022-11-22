/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

namespace emulator {

void q68ScreenInit(void);
void q68UpdatePixelBuffer(uint8_t *q68ScreenPtr, int length, int mode);
void q68RenderScreen(void);

}