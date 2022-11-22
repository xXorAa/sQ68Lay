/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <set>

#include <SDL.h>

#include "q68_screen.hpp"

namespace emulator {

struct qlColor {
        int r;
        int g;
        int b;
};

struct qlColor qlColors[16] = {
	{ 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xFF }, { 0xFF, 0x00, 0x00 },
	{ 0xFF, 0x00, 0xFF }, { 0x00, 0xFF, 0x00 }, { 0x00, 0xFF, 0xFF },
	{ 0xFF, 0xFF, 0x00 }, { 0xFF, 0xFF, 0xFF },
	{ 0x3f, 0x3f, 0x3f }, { 0x00, 0x00, 0x7f }, { 0x7f, 0x00, 0x00 },
	{ 0x7f, 0x00, 0x7f }, { 0x00, 0x7f, 0x00 }, { 0x00, 0x7f, 0x7f },
	{ 0x7f, 0x7f, 0x00 }, { 0x7f, 0x7f, 0x7f },
};

SDL_Window *q68Window;
Uint32 q68WindowId;
SDL_Renderer *q68Renderer;
SDL_Surface *q68Surface;
SDL_Texture *q68Texture;
Uint32 sdlColors[16];
SDL_Rect q68DestRect;
int q68Fullscreen = 0;
bool q68RenderScreenFlag = false;

void q68ScreenInit(void)
{
	Uint32 q68WindowMode;
	int i, w, h;

	auto q68VideoDriver = SDL_GetCurrentVideoDriver();
	
	SDL_DisplayMode q68Mode;
	SDL_GetCurrentDisplayMode(0, &q68Mode);

	std::cout << "Video Driver " <<
		q68VideoDriver <<
		" xres " <<
		q68Mode.w <<
		" yres " <<
		q68Mode.h <<
		std::endl;
	
	/* Ensure width and height are always initialised to sane values */
	w = 512;
	h = 256;

	std::set<std::string> desktops = {"x11", "cocoa", "windows", "wayland"};

	if (desktops.contains(q68VideoDriver)) {
		q68WindowMode = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

		//q68WindowMode |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		q68WindowMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	q68Window = SDL_CreateWindow("sQ68ux",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		w,
		h,
		q68WindowMode);

	if (q68Window == NULL) {
		throw std::runtime_error("Failed to Create Window");
	}

	q68WindowId = SDL_GetWindowID(q68Window);

	q68Renderer = SDL_CreateRenderer(q68Window,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if (q68Renderer == NULL) {
		throw std::runtime_error("Failed to Create Renderer");
	}

	SDL_RenderSetLogicalSize(q68Renderer, w, h);

	q68DestRect.x = q68DestRect.y = 0;
	q68DestRect.w = w;
	q68DestRect.h = h;

	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	q68Surface = SDL_CreateRGBSurfaceWithFormat(0,
		w,
		h,
		32,
		SDL_PIXELFORMAT_RGBA32);

	if (q68Surface == NULL) {
		throw std::runtime_error("Failed to Create Screen");
	}

	q68Texture = SDL_CreateTexture(q68Renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STREAMING,
		w,
		h);

	if (q68Texture == NULL) {
		throw std::runtime_error("Failed to Create Texture");
	}

	for (i = 0; i < 16; i++) {
		sdlColors[i] = SDL_MapRGB(q68Surface->format,
			qlColors[i].r,
			qlColors[i].g,
			qlColors[i].b);
	}
}

void q68UpdatePixelBuffer(uint8_t *q68ScreenPtr, int length, int mode)
{
	if (SDL_MUSTLOCK(q68Surface)) {
		SDL_LockSurface(q68Surface);
	}

	uint32_t *pixelPtr32 = (uint32_t *)q68Surface->pixels;
	uint8_t *q68ScreenPtrEnd = q68ScreenPtr + length;

	while (q68ScreenPtr < q68ScreenPtrEnd) {
		int t1 = *q68ScreenPtr++;
		int t2 = *q68ScreenPtr++;

		if (mode == 8) {
			for (int i = 0; i < 8; i += 2) {
				int color = ((t1 & 2) << 1) + ((t2 & 3)) +
					((t1 & 1) << 3);

				uint32_t x = sdlColors[color];

				*(pixelPtr32 + 7 - (i)) = x;
				*(pixelPtr32 + 7 - (i + 1)) = x;

				t1 >>= 2;
				t2 >>= 2;
			}
		} else {
			for (int i = 0; i < 8; i++) {
				int color = ((t1 & 1) << 2) + ((t2 & 1) << 1) +
					((t1 & 1) & (t2 & 1));

				uint32_t x = sdlColors[color];

				*(pixelPtr32 + 7 - i) = x;

				t1 >>= 1;
				t2 >>= 1;
			}
		}
		pixelPtr32 += 8;
	}

	if (SDL_MUSTLOCK(q68Surface)) {
		SDL_UnlockSurface(q68Surface);
	}
}

void q68RenderScreen(void)
{
	void *texture_buffer;
	int pitch;
	int w, h;
	SDL_PixelFormat pixelformat;

	SDL_UpdateTexture(q68Texture,
		NULL,
		q68Surface->pixels,
		q68Surface->pitch);
	SDL_RenderClear(q68Renderer);
	SDL_RenderCopyEx(q68Renderer,
		q68Texture,
		NULL,
		&q68DestRect,
		0,
		NULL,
		SDL_FLIP_NONE);
	SDL_RenderPresent(q68Renderer);
}

void q68FullScreen(void)
{
	q68Fullscreen ^= 1;

	SDL_SetWindowFullscreen(q68Window, q68Fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

}