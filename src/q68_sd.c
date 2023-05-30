/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <glib.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator_options.h"
#include "m68k.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

GMappedFile *sd1File = NULL;
uint8_t sd1CmdBuf[8];
int sd1CmdIdx = 0;
uint8_t sd1Resp = 0;
uint8_t sd1RespBuf[1 + 512 + 2];
bool sd1Multi = false;
int sd1Block;
int sd1RespIdx = 0;

GMappedFile * sd2File = NULL;

void q68InitSD(void)
{
	const char *sd1Filename = emulatorOptionString("sd1");
	if (strlen(sd1Filename) > 0) {
		printf("SD1: %s\n", sd1Filename);

		GError *err = NULL;
		sd1File = g_mapped_file_new(sd1Filename, true, &err);
		if (err != NULL) {
			fprintf(stderr, "Error: opening SD1 %s\n", err->message);
		}
		sd1CmdIdx = 0;
	}
}

void q68ProcessSDCmd(__attribute__ ((unused))int sd, uint8_t cmdByte)
{
	sd1CmdBuf[sd1CmdIdx++] = cmdByte;

	if (sd1CmdIdx < 7) {
		return;
	}

	if ((sd1CmdBuf[0] = 0xFF) &&
		(sd1CmdBuf[1] = 0x51) &&
		(sd1CmdBuf[6] = 0xFF)) {

		sd1CmdIdx = 0;

		sd1RespIdx = -1;
		sd1Resp = 0x00;
		sd1RespBuf[0] = 0xFE;

		uint32_t block = SDL_SwapBE32(*(uint32_t *)&sd1CmdBuf[2]);

		// Make sure we do not exceed mapped file limits
		if (((gsize)block * 512) < (g_mapped_file_get_length(sd1File) - 512)) {
			memcpy(&sd1RespBuf[1], g_mapped_file_get_contents(sd1File) + (block * 512), 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}
		sd1Multi = false;
	}

	if ((sd1CmdBuf[0] = 0xFF) &&
		(sd1CmdBuf[1] = 0x52) &&
		(sd1CmdBuf[6] = 0xFF)) {

		sd1CmdIdx = 0;

		sd1RespIdx = -1;
		sd1Resp = 0x00;
		sd1RespBuf[0] = 0xFE;

		uint32_t block = SDL_SwapBE32(*(uint32_t *)&sd1CmdBuf[2]);

		// Make sure we do not exceed mapped file limits
		if (((gsize)block * 512) < (g_mapped_file_get_length(sd1File) - 512)) {
			memcpy(&sd1RespBuf[1], g_mapped_file_get_contents(sd1File) + (block * 512), 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}

		sd1Block = block + 1;

		sd1Multi = true;
	}
}

uint8_t q68ProcessSDResponse(__attribute__ ((unused))int sd)
{
	if (sd1RespIdx < 0) {
		sd1RespIdx = 0;
		return sd1Resp;
	}

	if (sd1RespIdx < 515) {
		return sd1RespBuf[sd1RespIdx++];
	}

	if (sd1Multi) {
		sd1RespBuf[0] = 0xFE;

		if (((gsize)sd1Block * 512) < (g_mapped_file_get_length(sd1File) - 512)) {
			memcpy(&sd1RespBuf[1], g_mapped_file_get_contents(sd1File) + (sd1Block * 512), 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}
		sd1Block++;
		sd1RespIdx = 0;
		return sd1RespBuf[sd1RespIdx++];
	}
	return 0;
}
