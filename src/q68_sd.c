/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <fcntl.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "emulator_options.h"
#include "m68k.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

int sd1File = -1;
uint8_t sd1CmdBuf[8];
int sd1CmdIdx = 0;
uint8_t sd1RespBuf[2 + 512 +2];
int sd1RespIdx = 0;
int sd2File = -1;

void q68InitSD(void)
{
	const char *sd1Filename = emulatorOptionString("sd1");
	if (strlen(sd1Filename) > 0) {
		printf("SD1: %s\n", sd1Filename);
		sd1File = open(sd1Filename, O_RDWR | O_BINARY);
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

		sd1RespIdx = 0;
		sd1RespBuf[0] = 0x00;
		sd1RespBuf[1] = 0xFE;

		uint32_t block = SDL_SwapBE32(*(uint32_t *)&sd1CmdBuf[2]);

		lseek(sd1File, block * 512, SEEK_SET);
		read(sd1File, &sd1RespBuf[2], 512);
	}
}

uint8_t q68ProcessSDResponse(__attribute__ ((unused))int sd)
{
	if (sd1RespIdx < 516) {
		return sd1RespBuf[sd1RespIdx++];
	}

	return 0;
}
