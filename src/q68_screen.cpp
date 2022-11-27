/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <set>

#include <SDL.h>

#include "q68_hardware.hpp"
#include "q68_memory.hpp"
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
Uint32 sdlColors[16];
SDL_Rect q68DestRect;
int q68Fullscreen = 0;
bool q68RenderScreenFlag = false;
int q68CurrentMode;

struct q68Mode {
	uint32_t base;
	uint32_t size;
	int xRes;
	int yRes;
	SDL_Surface *surface;
	SDL_Texture *texture;
};

struct q68Mode q68Modes[8] = {
{ 0x00020000, 32_KiB, 512, 256  },      // ql mode 8                0
{ 0x00020000, 32_KiB, 512, 256  },      // ql mode 4                1
{ 0xFE800000, 1_MiB, 512, 256    },      // small 16 bit screen      2
{ 0xFE800000, 2_MiB, 1024, 512   },      // large 16 bit screen      3
{ 0xFE800000, 192_KiB, 1024, 768 },      // large QL Mode 4 screen   4
{ 0xFE800000, 1_MiB, 1024, 768   },      // auora 8 bit              5
{ 0xFE800000, 1_MiB, 512, 384    },      // medium 16 bit screen     6
{ 0xFE800000, 2_MiB, 1024, 768   }       // huge 16 bit              7
};

void q68CreateSurface(int xRes, int yRes)
{
	std::cout << "CreateSurface " << xRes << " " << yRes << std::endl;

}

void q68ScreenInit(int q68Mode)
{
	q68CurrentMode = q68Mode;

	// Fixed screen res for Q68 output
	int xRes = 1024;
	int yRes = 768;

	q68DestRect.x = q68DestRect.y = 0;
	q68DestRect.w = xRes;
	q68DestRect.h = yRes;

	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	auto q68VideoDriver = SDL_GetCurrentVideoDriver();

	SDL_DisplayMode q68DisplayMode;
	SDL_GetCurrentDisplayMode(0, &q68DisplayMode);

	std::cout << "Video Driver " <<
		q68VideoDriver <<
		" xres " <<
		q68DisplayMode.w <<
		" yres " <<
		q68DisplayMode.h <<
		std::endl;

	std::set<std::string> desktops = {"x11", "cocoa", "windows", "wayland", "emscripten"};

	Uint32 q68WindowMode;

	if (desktops.contains(q68VideoDriver)) {
		q68WindowMode = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

		//q68WindowMode |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		q68WindowMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	q68Window = SDL_CreateWindow("sQ68ux",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		xRes,
		yRes,
		q68WindowMode);

	if (q68Window == NULL) {
		throw std::runtime_error("Failed to Create Window");
	}

	q68Renderer = SDL_CreateRenderer(q68Window,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if (q68Renderer == NULL) {
		throw std::runtime_error("Failed to Create Renderer");
	}

	SDL_RenderSetLogicalSize(q68Renderer, xRes, yRes);

	for (int i = 0; i < 8; i++) {
		q68Modes[i].surface = SDL_CreateRGBSurfaceWithFormat(0,
			q68Modes[i].xRes,
			q68Modes[i].yRes,
			32,
			SDL_PIXELFORMAT_RGBA32);

		if (q68Modes[i].surface == NULL) {
			throw std::runtime_error("Failed to Create Screen");
		}

		q68Modes[i].texture = SDL_CreateTexture(q68Renderer,
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING,
			q68Modes[i].xRes,
			q68Modes[i].yRes);

		if (q68Modes[i].texture == NULL) {
			throw std::runtime_error("Failed to Create Texture");
		}
	}

	for (int i = 0; i < 16; i++) {
		sdlColors[i] = SDL_MapRGB(q68Modes[q68Mode].surface->format,
			qlColors[i].r,
			qlColors[i].g,
			qlColors[i].b);
	}
}

void q68ScreenChangeMode(int q68Mode)
{
	q68CurrentMode = q68Mode;
}

void q68UpdatePixelBuffer()
{
	if (SDL_MUSTLOCK(q68Modes[q68CurrentMode].surface)) {
		SDL_LockSurface(q68Modes[q68CurrentMode].surface);
	}

	uint32_t *pixelPtr32 = (uint32_t *)q68Modes[q68CurrentMode].surface->pixels + 7;
	uint8_t *q68ScreenPtr;
	uint8_t *q68ScreenPtrEnd;

	switch (q68CurrentMode) {
	case 0:
	case 1:
		q68ScreenPtr = q68MemorySpace + q68Modes[q68CurrentMode].base;
		q68ScreenPtrEnd = q68ScreenPtr + q68Modes[q68CurrentMode].size;
		break;
	case 4:
		q68ScreenPtr = q68ScreenSpace;
		q68ScreenPtrEnd = q68ScreenPtr + q68Modes[q68CurrentMode].size;
		break;
	}

	while (q68ScreenPtr < q68ScreenPtrEnd) {
		int t1 = *q68ScreenPtr++;
		int t2 = *q68ScreenPtr++;

		switch (q68CurrentMode) {
		case 0:
			for (int i = 0; i < 8; i += 2) {
				int color = ((t1 & 2) << 1) + ((t2 & 3)) +
					((t1 & 1) << 3);

				uint32_t x = sdlColors[color];

				*pixelPtr32-- = x;
				*pixelPtr32-- = x;

				t1 >>= 2;
				t2 >>= 2;
			}
			break;
		case 1:
		case 4:
			for (int i = 0; i < 8; i++) {
				int color = ((t1 & 1) << 2) + ((t2 & 1) << 1) +
					((t1 & 1) & (t2 & 1));

				uint32_t x = sdlColors[color];

				*pixelPtr32-- = x;

				t1 >>= 1;
				t2 >>= 1;
			}
			break;
		default:
			std::cerr << "Unsuported Mode: " << (int)emulator::q68_q68_dmode << std::endl;
		}
		pixelPtr32 += 16;
	}

	if (SDL_MUSTLOCK(q68Modes[q68CurrentMode].surface)) {
		SDL_UnlockSurface(q68Modes[q68CurrentMode].surface);
	}
}

void q68RenderScreen(void)
{
	void *texture_buffer;
	int pitch;
	int w, h;
	SDL_PixelFormat pixelformat;

	SDL_UpdateTexture(q68Modes[q68CurrentMode].texture,
		NULL,
		q68Modes[q68CurrentMode].surface->pixels,
		q68Modes[q68CurrentMode].surface->pitch);
	SDL_RenderClear(q68Renderer);
	SDL_RenderCopy(q68Renderer,
		q68Modes[q68CurrentMode].texture,
		NULL,
		NULL);
	SDL_RenderPresent(q68Renderer);
}

void q68FullScreen(void)
{
	q68Fullscreen ^= 1;

	SDL_SetWindowFullscreen(q68Window, q68Fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

}