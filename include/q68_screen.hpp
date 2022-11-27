/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

namespace emulator {

extern bool q68RenderScreenFlag;

void q68ScreenInit(int q68Mode);
void q68UpdatePixelBuffer();
void q68RenderScreen(void);
void q68ScreenChangeMode(int q68Mode);

}