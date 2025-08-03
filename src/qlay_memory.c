/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>

#include "emulator_files.h"
#include "emulator_hardware.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "m68k.h"
#include "qlay_disk.h"
#include "utarray.h"

static Uint8 *qlayMemSpace = NULL;
static unsigned int qlayMemSize = 0;
static unsigned int qlayRamSize = 0;
static unsigned int qlayRomLow = 0;

typedef struct {
	char *romname;
	Uint32 romaddr;
} exprom_t;

static const UT_icd exprom_icd = { sizeof(exprom_t), NULL, NULL, NULL };

// how many extra cycles accessing screen ram costs
#define CONTENTION_CYCLES 3

Uint8 *emulatorMemorySpace(void)
{
	return qlayMemSpace;
}

Uint8 *emulatorScreenSpace(void)
{
	return NULL;
}

int emulatorInitMemory(void)
{
	int i;
	unsigned int minRomAddr = 0;
	unsigned int maxRomAddr = 0;

	UT_array *expromArray;
	utarray_new(expromArray, &exprom_icd);

	// TODO: add proper exprom handling
	int expromCount = emulatorOptionDevCount("exprom");
	for (i = 0; i < expromCount; i++) {
		const char *rom = emulatorOptionDev("exprom", i);
		const char *romat = SDL_strchr(rom, '@');

		if (romat == NULL) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				     "No address for rom %i %s, rom ignored", i,
				     rom);
			continue;
		}

		Uint32 romAddr = SDL_strtol(romat + 1, NULL, 16);
		if (romAddr == 0) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Invalid address for rom %i %s, rom ignored", i,
				rom);
			continue;
		}

		if (romAddr % KB(16)) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Address must be on 16KB boundary %i %s, rom ignored",
				i, rom);
			continue;
		}

		// don't allow the rom inside the IO or screen RAM
		if ((romAddr >= QL_INTERNAL_IO) && (romAddr < KB(256))) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Invalid address for rom 0x%8.8X %s, rom ignored",
				romAddr, rom);
			continue;
		}

		exprom_t expromItem;
		expromItem.romname = SDL_strndup(rom, romat - rom);
		expromItem.romaddr = romAddr;

		utarray_push_back(expromArray, &expromItem);

		// update the minimum rom address if it is going to shrink memory
		if (romAddr >= KB(128)) {
			if (minRomAddr == 0) {
				minRomAddr = romAddr;
			} else if (romAddr < minRomAddr) {
				minRomAddr = romAddr;
			}

			if (maxRomAddr < romAddr) {
				maxRomAddr = romAddr;
			}
		}
	}

	// account for the rom slot size
	maxRomAddr += KB(16);

	// ROM 64K and IO space 64K added to ramsize
	qlayRamSize = KB(emulatorOptionInt("ramsize")) + KB(128);

	if (qlayRamSize < KB(256)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Ramsize Error too small %u\n", qlayMemSize);
		return 1;
	}

	if ((minRomAddr >= KB(256)) && (qlayRamSize > minRomAddr)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Ramsize overlaps with rom %u %u\n", qlayRamSize,
			     minRomAddr);
		return 1;
	}

	qlayMemSize = maxRomAddr > qlayRamSize ? maxRomAddr : qlayRamSize;

	qlayMemSpace = SDL_calloc(qlayMemSize, 1);
	if (qlayMemSpace == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "malloc failed %s %d", __FILE__, __LINE__);
		return 1;
	}

	// load the roms now
	exprom_t *expromItem = NULL;
	while ((expromItem =
			(exprom_t *)utarray_next(expromArray, expromItem))) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
			    "Loading Expansion ROM %s at 0x%X\n",
			    expromItem->romname, expromItem->romaddr);
		emulatorLoadFile(expromItem->romname,
				 &emulatorMemorySpace()[expromItem->romaddr],
				 0);

		free(expromItem->romname);
		expromItem->romname = NULL;
	}
	utarray_free(expromArray);

	// set the lowest rom address
	if (minRomAddr == 0) {
		qlayRomLow = qlayMemSize;
	} else {
		qlayRomLow = minRomAddr;
	}

	emulatorLoadFile(emulatorOptionString("sysrom"),
			 &emulatorMemorySpace()[0], 0);

	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Initialzed RAM %uk\n",
		    (qlayRamSize / 1024) - 128);

	return 0;
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if ((address >= QL_INTERNAL_IO) &&
	    address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
		return qlHardwareRead8(address);
	}

	if ((QLAY_NFA_IO) && address < (QLAY_NFA_IO + QLAY_NFA_IO_SIZE)) {
		return rdnfa(address);
	}

	if (address >= qlayMemSize) {
		return 0;
	}

	if ((address >= qlayRamSize) && (address < qlayRomLow)) {
		return 0;
	}

	// Add some contension for BBQL ram
	if ((address >= KB(128)) && (address < KB(256))) {
		extraCycles += CONTENTION_CYCLES;
	}

	return qlayMemSpace[address];
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	extraCycles += 4;
	return m68k_read_memory_8(address) << 8 |
	       m68k_read_memory_8(address + 1);
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
	if (address >= qlayMemSize) {
		return 0;
	}

	return SDL_Swap16BE(*(Uint16 *)&qlayMemSpace[address]);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	extraCycles += 12;
	return m68k_read_memory_8(address) << 24 |
	       m68k_read_memory_8(address + 1) << 16 |
	       m68k_read_memory_8(address + 2) << 8 |
	       m68k_read_memory_8(address + 3);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
	if (address >= qlayMemSize) {
		return 0;
	}

	return SDL_Swap32BE(*(Uint32 *)&qlayMemSpace[address]);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	if (address < QL_INTERNAL_IO) {
		return;
	}

	if ((address >= QL_INTERNAL_IO) &&
	    address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
		qlHardwareWrite8(address, value);
		return;
	}

	if ((address >= QLAY_NFA_IO) &&
	    address < (QLAY_NFA_IO + QLAY_NFA_IO_SIZE)) {
		wrnfa(address, value);
		qlayMemSpace[address] = value;
	}

	if (qsound_enabled && (address >= qsound_addr) &&
	    (address < (qsound_addr + 4))) {
		qsoundWrite(address, value);
		return;
	}

	if (address >= qlayRamSize) {
		return;
	}

	if ((address >= KB(128)) && (address < KB(256))) {
		extraCycles += CONTENTION_CYCLES;
	}

	qlayMemSpace[address] = value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	extraCycles += 4;
	m68k_write_memory_8(address + 0, (value >> 8) & 0xFF);
	m68k_write_memory_8(address + 1, (value >> 0) & 0xFF);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	extraCycles += 12;
	m68k_write_memory_8(address + 0, (value >> 24) & 0xFF);
	m68k_write_memory_8(address + 1, (value >> 16) & 0xFF);
	m68k_write_memory_8(address + 2, (value >> 8) & 0xFF);
	m68k_write_memory_8(address + 3, (value >> 0) & 0xFF);
}
