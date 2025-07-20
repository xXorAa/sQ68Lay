/*
 * Copyright (c) 2020-2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "emulator_hardware.h"
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
	{ 0xF8, 0xF8, 0x00 }, { 0xF8, 0xF8, 0xF8 }, { 0x3f, 0x3f, 0x3f },
	{ 0x00, 0x00, 0x7f }, { 0x7f, 0x00, 0x00 }, { 0x7f, 0x00, 0x7f },
	{ 0x00, 0x7f, 0x00 }, { 0x00, 0x7f, 0x7f }, { 0x7f, 0x7f, 0x00 },
	{ 0x7f, 0x7f, 0x7f },
};

SDL_Window *emulatorWindow;
Uint32 emulatorWindowId;
SDL_Renderer *emulatorRenderer;
Uint32 sdlColors[16];
SDL_Rect emulatorDestRect;
int emulatorFullscreen = 0;
bool emulatorRenderScreenFlag = false;
int emulatorCurrentMode;
bool emulatorSecondScreen = false;

static const char *emulatorName = EMU_STR;

struct qlMode {
	uint32_t base;
	uint32_t size;
	int xRes;
	int yRes;
	SDL_Surface *surface;
	SDL_Texture *texture;
};

struct qlMode qlModes[8] = {
	{ 0x00020000, KB(32), 512, 256, NULL,
	  NULL }, // ql mode 8              0
	{ 0x00020000, KB(32), 512, 256, NULL,
	  NULL }, // ql mode 4              1
	{ 0xFE800000, KB(256), 512, 256, NULL,
	  NULL }, // small 16 bit screen    2
	{ 0xFE800000, MB(1), 1024, 512, NULL,
	  NULL }, // large 16 bit screen    3
	{ 0xFE800000, KB(192), 1024, 768, NULL,
	  NULL }, // large QL Mode 4 screen 4
	{ 0xFE800000, KB(512), 1024, 768, NULL,
	  NULL }, // auora 8 bit            5
	{ 0xFE800000, KB(384), 512, 384, NULL,
	  NULL }, // medium 16 bit screen   6
	{ 0xFE800000, MB(1) + KB(512), 1024, 768, NULL,
	  NULL } // huge 16 bit            7
};

const char *const desktops = "x11,cocoa,windows,wayland,emscripten";

int emulatorScreenInit(int emulatorMode)
{
	// Fixed screen res for emulator output
	int xRes = 1024;
	int yRes = 768;

	emulatorCurrentMode = emulatorMode;

	emulatorDestRect.x = emulatorDestRect.y = 0;
	emulatorDestRect.w = xRes;
	emulatorDestRect.h = yRes;

	emulatorWindow = SDL_CreateWindow(emulatorName, xRes, yRes,
					  SDL_WINDOW_RESIZABLE);

	if (emulatorWindow == NULL) {
		fprintf(stderr, "Failed to Create Window %s\n", SDL_GetError());
		return 1;
	}

	emulatorRenderer = SDL_CreateRenderer(emulatorWindow, NULL);

	if (emulatorRenderer == NULL) {
		fprintf(stderr, "Failed to Create Renderer %s\n",
			SDL_GetError());
		return 1;
	}

	SDL_SetRenderLogicalPresentation(emulatorRenderer, xRes, yRes,
					 SDL_LOGICAL_PRESENTATION_LETTERBOX);

	SDL_RenderClear(emulatorRenderer);
	SDL_RenderPresent(emulatorRenderer);

	for (int i = 0; i < 8; i++) {
		qlModes[i].surface = SDL_CreateSurface(qlModes[i].xRes,
						       qlModes[i].yRes,
						       SDL_PIXELFORMAT_RGBA32);

		if (qlModes[i].surface == NULL) {
			fprintf(stderr, "Failed to Create Screen %s\n",
				SDL_GetError());
			return 1;
		}

		qlModes[i].texture = SDL_CreateTexture(
			emulatorRenderer, SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING, qlModes[i].xRes,
			qlModes[i].yRes);

		if (qlModes[i].texture == NULL) {
			fprintf(stderr, "Failed to Create Texture %s\n",
				SDL_GetError());
			return 1;
		}
	}

	for (int i = 0; i < 16; i++) {
		sdlColors[i] = SDL_MapRGB(
			SDL_GetPixelFormatDetails(qlModes[0].surface->format),
			NULL, qlColors[i].r, qlColors[i].g, qlColors[i].b);
	}

	return 0;
}

void emulatorScreenChangeMode(int emulatorMode)
{
	emulatorCurrentMode = emulatorMode;
}

// frame counter for flash
static int curframe = 0;

void emulatorUpdatePixelBufferQL(uint8_t *emulatorScreenPtr,
				 uint8_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 =
		(uint32_t *)qlModes[emulatorCurrentMode].surface->pixels;
	int curpix = 0;
	uint32_t flashbg = 0;
	int flashon = 0;

	while (emulatorScreenPtr < emulatorScreenPtrEnd) {
		uint8_t t1 = *emulatorScreenPtr++;
		uint8_t t2 = *emulatorScreenPtr++;

		switch (emulatorCurrentMode) {
		case 0:
			for (int i = 6; i > -2; i -= 2) {
				uint8_t p1 = (t1 >> i) & 0x03;
				uint8_t p2 = (t2 >> i) & 0x03;

				int color = ((p1 & 2) << 1) + ((p2 & 3));
				int flashbit = (p1 & 1);

				uint32_t x = sdlColors[color];

				if ((curframe & BIT(5)) && flashon) {
					x = flashbg;
				}

				*pixelPtr32++ = x;
				*pixelPtr32++ = x;

				// flash happens after the pixel
				if (flashbit) {
					if (flashon == 0) {
						flashbg = x;
						flashon = 1;
					} else {
						flashon = 0;
					}
				}

				// Handle flash end of line
				// stride is fixed at 256 because mode 8
				// is fixed at that size on QL and Q68
				curpix++;
				curpix %= 256;
				if (curpix == 0) {
					flashbg = 0;
					flashon = 0;
				}
			}
			break;
		case 1:
		case 4:
			for (int i = 7; i > -1; i--) {
				uint8_t p1 = (t1 >> i) & 0x01;
				uint8_t p2 = (t2 >> i) & 0x01;

				int color = ((p1 & 1) << 2) + ((p2 & 1) << 1) +
					    ((p1 & 1) & (p2 & 1));

				uint32_t x = sdlColors[color];

				*pixelPtr32++ = x;
			}
			break;
		}
	}

	// frame counter for flash
	curframe++;
	curframe %= 64;
}

void emulatorUpdatePixelBuffer33(uint16_t *emulatorScreenPtr,
				 uint16_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 =
		(uint32_t *)qlModes[emulatorCurrentMode].surface->pixels;

	while (emulatorScreenPtr < emulatorScreenPtrEnd) {
		uint16_t pixel16 = SDL_Swap16BE(*emulatorScreenPtr++);

		// red
		uint8_t red = (pixel16 & 0x07C0) >> 3;

		// green
		uint8_t green = (pixel16 & 0xF800) >> 8;

		// blue
		uint8_t blue = (pixel16 & 0x003E) << 2;

		uint32_t pixel32 = SDL_MapRGB(
			SDL_GetPixelFormatDetails(
				qlModes[emulatorCurrentMode].surface->format),
			NULL, red, green, blue);

		*pixelPtr32++ = pixel32;
	}
}

void emulatorUpdatePixelBufferAurora(uint8_t *emulatorScreenPtr,
				     uint8_t *emulatorScreenPtrEnd)
{
	uint32_t *pixelPtr32 =
		(uint32_t *)qlModes[emulatorCurrentMode].surface->pixels;

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

		uint32_t pixel32 = SDL_MapRGB(
			SDL_GetPixelFormatDetails(
				qlModes[emulatorCurrentMode].surface->format),
			NULL, red, green, blue);

		*pixelPtr32++ = pixel32;
	}
}

void emulatorUpdatePixelBuffer(void)
{
	if (SDL_MUSTLOCK(qlModes[emulatorCurrentMode].surface)) {
		SDL_LockSurface(qlModes[emulatorCurrentMode].surface);
	}

	uint8_t *emulatorScreenPtr;
	uint8_t *emulatorScreenPtrEnd;

	switch (emulatorCurrentMode) {
	case 0:
	case 1:
		emulatorScreenPtr = emulatorMemorySpace() +
				    qlModes[emulatorCurrentMode].base;
		emulatorScreenPtrEnd =
			emulatorScreenPtr + qlModes[emulatorCurrentMode].size;

		// handle the second QL screen
		if (emulatorSecondScreen) {
			emulatorScreenPtr += KB(32);
			emulatorScreenPtrEnd += KB(32);
		}

		emulatorUpdatePixelBufferQL(emulatorScreenPtr,
					    emulatorScreenPtrEnd);
		break;
	case 2:
	case 3:
	case 6:
	case 7:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd =
			emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBuffer33((uint16_t *)emulatorScreenPtr,
					    (uint16_t *)emulatorScreenPtrEnd);
		break;
	case 4:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd =
			emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBufferQL(emulatorScreenPtr,
					    emulatorScreenPtrEnd);
		break;
	case 5:
		emulatorScreenPtr = emulatorScreenSpace();
		emulatorScreenPtrEnd =
			emulatorScreenPtr + qlModes[emulatorCurrentMode].size;
		emulatorUpdatePixelBufferAurora(emulatorScreenPtr,
						emulatorScreenPtrEnd);
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
	SDL_UpdateTexture(qlModes[emulatorCurrentMode].texture, NULL,
			  qlModes[emulatorCurrentMode].surface->pixels,
			  qlModes[emulatorCurrentMode].surface->pitch);
	SDL_RenderClear(emulatorRenderer);
	SDL_RenderTexture(emulatorRenderer,
			  qlModes[emulatorCurrentMode].texture, NULL, NULL);
	SDL_RenderPresent(emulatorRenderer);
}

void emulatorFullScreen(void)
{
	emulatorFullscreen ^= 1;

	SDL_SetWindowFullscreen(emulatorWindow,
				emulatorFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
}
