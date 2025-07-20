/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <errno.h>
#include <fcntl.h>
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "emulator_files.h"
#include "emulator_options.h"
#include "m68k.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

int sd1Fd;
size_t sd1FileSize;
bool sd1Present = false;
uint8_t sd1CmdBuf[8];
int sd1CmdIdx = 0;
uint8_t sd1Resp = 0;
uint8_t sd1RespBuf[1 + 512 + 2];
bool sd1Multi = false;
int sd1Block;
int sd1RespIdx = 0;

void q68InitSD(void)
{
	const char *sd1Filename = emulatorOptionString("sd1");
	if (strlen(sd1Filename) > 0) {
		printf("SD1: %s\n", sd1Filename);
		if (!emulatorFileExists(sd1Filename)) {
			fprintf(stderr, "Error: File not Found %s\n",
				sd1Filename);
			sd1Present = false;
			return;
		}

		sd1FileSize = emulatorFileSize(sd1Filename);
		if (sd1FileSize == 0) {
			fprintf(stderr, "Error: File zero sized %s\n",
				sd1Filename);
			sd1Present = false;
			return;
		}

		sd1Fd = open(sd1Filename, O_RDWR);

		sd1CmdIdx = 0;
		sd1Present = true;
	} else {
		sd1Present = false;
	}
}

void q68ProcessSDCmd(__attribute__((unused)) int sd, uint8_t cmdByte)
{
	if (!sd1Present) {
		return;
	}

	sd1CmdBuf[sd1CmdIdx++] = cmdByte;

	if (sd1CmdIdx < 7) {
		return;
	}

	if ((sd1CmdBuf[0] = 0xFF) && (sd1CmdBuf[1] = 0x51) &&
	    (sd1CmdBuf[6] = 0xFF)) {
		sd1CmdIdx = 0;

		sd1RespIdx = -1;
		sd1Resp = 0x00;
		sd1RespBuf[0] = 0xFE;

		uint32_t block = SDL_Swap32BE(*(uint32_t *)&sd1CmdBuf[2]);

		// Make sure we do not exceed mapped file limits
		if (((size_t)block * 512) < (sd1FileSize)-512) {
			lseek(sd1Fd, block * 512, SEEK_SET);
			read(sd1Fd, &sd1RespBuf[1], 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}
		sd1Multi = false;
	}

	if ((sd1CmdBuf[0] = 0xFF) && (sd1CmdBuf[1] = 0x52) &&
	    (sd1CmdBuf[6] = 0xFF)) {
		sd1CmdIdx = 0;

		sd1RespIdx = -1;
		sd1Resp = 0x00;
		sd1RespBuf[0] = 0xFE;

		uint32_t block = SDL_Swap32BE(*(uint32_t *)&sd1CmdBuf[2]);

		// Make sure we do not exceed mapped file limits
		if (((size_t)block * 512) < (sd1FileSize - 512)) {
			//memcpy(&sd1RespBuf[1], sd1File + (block * 512), 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}

		sd1Block = block + 1;

		sd1Multi = true;
	}
}

uint8_t q68ProcessSDResponse(__attribute__((unused)) int sd)
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

		if (((size_t)sd1Block * 512) < (sd1FileSize - 512)) {
			lseek(sd1Fd, sd1Block * 512, SEEK_SET);
			read(sd1Fd, &sd1RespBuf[1], 512);
		} else {
			memset(&sd1RespBuf[1], 0, 512);
		}
		sd1Block++;
		sd1RespIdx = 0;
		return sd1RespBuf[sd1RespIdx++];
	}
	return 0;
}
