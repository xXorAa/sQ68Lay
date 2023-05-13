/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>
#include <stdbool.h>

#include "emulator_memory.h"
#include "emulator_options.h"

struct qlColor {
        int r;
        int g;
        int b;
};

struct qlColor qlColors[16] = {
	{ 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xF8 }, { 0xF8, 0x00, 0x00 },
	{ 0xF8, 0x00, 0xF8 }, { 0x00, 0xF8, 0x00 }, { 0x00, 0xF8, 0xF8 },
	{ 0xF8, 0xF8, 0x00 }, { 0xF8, 0xF8, 0xF8 },
	{ 0x3f, 0x3f, 0x3f }, { 0x00, 0x00, 0x7f }, { 0x7f, 0x00, 0x00 },
	{ 0x7f, 0x00, 0x7f }, { 0x00, 0x7f, 0x00 }, { 0x00, 0x7f, 0x7f },
	{ 0x7f, 0x7f, 0x00 }, { 0x7f, 0x7f, 0x7f },
};

SDL_Window *emulatorWindow;
Uint32 emulatorWindowId;
SDL_Renderer *emulatorRenderer;
Uint32 sdlColors[16];
SDL_Rect emulatorDestRect;
int emulatorFullscreen = 0;
bool emulatorRenderScreenFlag = false;
int emulatorCurrentMode;

static const char* emulatorName = EMU_STR;

struct qlMode {
	uint32_t base;
	uint32_t size;
	int xRes;
	int yRes;
	SDL_Surface *surface;
	SDL_Texture *texture;
};

struct qlMode qlModes[8] = {
{ 0x00020000, KB(32), 512, 256           }, // ql mode 8              0
{ 0x00020000, KB(32), 512, 256           }, // ql mode 4              1
{ 0xFE800000, KB(256), 512, 256          }, // small 16 bit screen    2
{ 0xFE800000, MB(1), 1024, 512           }, // large 16 bit screen    3
{ 0xFE800000, KB(192), 1024, 768         }, // large QL Mode 4 screen 4
{ 0xFE800000, KB(512), 1024, 768         }, // auora 8 bit            5
{ 0xFE800000, KB(384), 512, 384          }, // medium 16 bit screen   6
{ 0xFE800000, MB(1) + KB(512), 1024, 768 }  // huge 16 bit            7
};

const char* const desktops = "x11,cocoa,windows,wayland,emscripten";

int emulatorScreenInit(int emulatorMode)
{
	// Fixed screen res for emulator output
	int xRes = 1024;
	int yRes = 768;
	const char *emulatorVideoDriver;
	uint32_t emulatorWindowMode;

	emulatorCurrentMode = emulatorMode;

	emulatorDestRect.x = emulatorDestRect.y = 0;
	emulatorDestRect.w = xRes;
	emulatorDestRect.h = yRes;

	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	emulatorVideoDriver = SDL_GetCurrentVideoDriver();

	SDL_DisplayMode emulatorDisplayMode;
	SDL_GetCurrentDisplayMode(0, &emulatorDisplayMode);

	printf("VideoDriver %s xres %d yres %d\n",
		emulatorVideoDriver,
		emulatorDisplayMode.w,
		emulatorDisplayMode.h);

	if (strstr(desktops, emulatorVideoDriver)) {
		emulatorWindowMode = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	} else {
		emulatorWindowMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	emulatorWindow = SDL_CreateWindow(emulatorName,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		xRes,
		yRes,
		emulatorWindowMode);

	if (emulatorWindow == NULL) {
		fprintf(stderr, "Failed to Create Window %s\n", SDL_GetError());
		return 1;
	}

	emulatorRenderer = SDL_CreateRenderer(emulatorWindow,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if (emulatorRenderer == NULL) {
		fprintf(stderr, "Failed to Create Renderer %s\n", SDL_GetError());
		return 1;
	}

	SDL_RenderSetLogicalSize(emulatorRenderer, xRes, yRes);

	SDL_RenderClear(emulatorRenderer);
	SDL_RenderPresent(emulatorRenderer);

	for (int i = 0; i < 8; i++) {
		qlModes[i].surface = SDL_CreateRGBSurfaceWithFormat(0,
			qlModes[i].xRes,
			qlModes[i].yRes,
			32,
			SDL_PIXELFORMAT_RGBA32);

		if (qlModes[i].surface == NULL) {
			fprintf(stderr, "Failed to Create Screen %s\n", SDL_GetError());
			return 1;
		}

		qlModes[i].texture = SDL_CreateTexture(emulatorRenderer,
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING,
			qlModes[i].xRes,
			qlModes[i].yRes);

		if (qlModes[i].texture == NULL) {
			fprintf(stderr, "Failed to Create Texture %s\n", SDL_GetError());
			return 1;
		}
	}

	for (int i = 0; i < 16; i++) {
		sdlColors[i] = SDL_MapRGB(qlModes[0].surface->format,
			qlColors[i].r,
			qlColors[i].g,
			qlColors[i].b);
	}

	return 0;
}

void emulatorScreenChangeMode(int emulatorMode)
{
	emulatorCurrentMode = emulatorMode;
}

void emulatorUpdatePixelBufferQL(uint8_t *emulatorScreenPtr, uint8_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 = (uint32_t *)qlModes[emulatorCurrentMode].surface->pixels + 7;

	while (emulatorScreenPtr < emulatorScreenPtrEnd) {
		int t1 = *emulatorScreenPtr++;
		int t2 = *emulatorScreenPtr++;

		switch (emulatorCurrentMode) {
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
		}
		pixelPtr32 += 16;
	}
}

void emulatorUpdatePixelBuffer33(uint16_t *emulatorScreenPtr, uint16_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 = (uint32_t *)qlModes[emulatorCurrentMode].surface->pixels;

	while (emulatorScreenPtr < emulatorScreenPtrEnd) {
		uint16_t pixel16 = SDL_SwapBE16(*emulatorScreenPtr++);

		// red
		uint8_t red = (pixel16 & 0x07C0) >> 3;

		// green
		uint8_t green = (pixel16 & 0xF800) >> 8;

		// blue
		uint8_t blue = (pixel16 & 0x003E) << 2;

		uint32_t pixel32 = SDL_MapRGB(qlModes[emulatorCurrentMode].surface->format, red, green, blue);

		*pixelPtr32++ = pixel32;
	}
}

void emulatorUpdatePixelBufferAurora(uint8_t *emulatorScreenPtr, uint8_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 = (uint32_t *)qlModes[emulatorCurrentMode].surface->pixels;

	while (emulatorScreenPtr < emulatorScreenPtrEnd) {
		uint8_t pixel8 = *emulatorScreenPtr++;

		// red
		uint8_t red = (pixel8 & (1 << 6)) >> 4;
		red |= pixel8 & (1 << 3) >> 2;
		red |= pixel8 & 1;
		red *= 35;

		// green
		uint8_t green = (pixel8 & (1 << 7)) >> 5;
		green |= (pixel8 & (1 << 4)) >> 3;
		green |= (pixel8 & (1 << 1)) >> 1;
		green *= 35;

		// blue
		uint8_t blue = (pixel8 & (1 << 5)) >> 3;
		blue |= (pixel8 & (1 << 2)) >> 1;
		blue |= pixel8 & 1;
		blue *= 35;

		uint32_t pixel32 = SDL_MapRGB(qlModes[emulatorCurrentMode].surface->format, red, green, blue);

		*pixelPtr32++ = pixel32;
	}
}

void emulatorUpdatePixelBuffer()
{
	if (SDL_MUSTLOCK(qlModes[emulatorCurrentMode].surface)) {
		SDL_LockSurface(qlModes[emulatorCurrentMode].surface);
	}

	uint8_t *emulatorScreenPtr;
	uint8_t *emulatorScreenPtrEnd;

	switch (emulatorCurrentMode) {
	case 0:
	case 1:
		emulatorScreenPtr = emulatorMemorySpace() + qlModes[emulatorCurrentMode].base;
		emulatorScreenPtrEnd = emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBufferQL(emulatorScreenPtr, emulatorScreenPtrEnd);
		break;
	case 2:
	case 3:
	case 6:
	case 7:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd = emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBuffer33((uint16_t *)emulatorScreenPtr, (uint16_t *)emulatorScreenPtrEnd);
		break;
	case 4:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd = emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBufferQL(emulatorScreenPtr, emulatorScreenPtrEnd);
		break;
	case 5:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd = emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBufferAurora(emulatorScreenPtr, emulatorScreenPtrEnd);
		break;
	default:
		fprintf(stderr, "Unsupported Screen Mode\n");
	}

	if (SDL_MUSTLOCK(qlModes[emulatorCurrentMode].surface)) {
		SDL_UnlockSurface(qlModes[emulatorCurrentMode].surface);
	}
}

void emulatorRenderScreen(void)
{
	void *texture_buffer;
	int pitch;
	int w, h;
	SDL_PixelFormat pixelformat;

	SDL_UpdateTexture(qlModes[emulatorCurrentMode].texture,
		NULL,
		qlModes[emulatorCurrentMode].surface->pixels,
		qlModes[emulatorCurrentMode].surface->pitch);
	SDL_RenderClear(emulatorRenderer);
	SDL_RenderCopy(emulatorRenderer,
		qlModes[emulatorCurrentMode].texture,
		NULL,
		NULL);
	SDL_RenderPresent(emulatorRenderer);
}

void emulatorFullScreen(void)
{
	emulatorFullscreen ^= 1;

	SDL_SetWindowFullscreen(emulatorWindow, emulatorFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}
