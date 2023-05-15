/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "emulator_hardware.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "m68k.h"

static uint8_t *qlayMemSpace = NULL;
static size_t qlayMemSize = 0;

uint8_t *emulatorMemorySpace(void)
{
	return qlayMemSpace;
}

uint8_t *emulatorScreenSpace(void)
{
	return NULL;
}

int emulatorInitMemory(void)
{
	// ROM 64K and IO space 64K added to ramsize
	qlayMemSize = KB(emulatorOptionInt("ramsize")) + KB(128);

	if (qlayMemSize < KB(256)) {
		fprintf(stderr, "Ramsize Error too small %zu\n", qlayMemSize);
		return 1;
	}

	qlayMemSpace = calloc(qlayMemSize, 1);
	if (qlayMemSpace == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __func__);
		return 1;
	}

	return 0;
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if ((address >= QL_INTERNAL_IO) &&
	    address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
		return qlHardwareRead8(address);
	}

	/*
	if ((QLAY_NFA_IO) &&
	    address < (QLAY_NFA_IO + QLAY_NFA_IO_SIZE)) {
		return nfaRead8(address);
	}
	*/
	if (address >= qlayMemSize) {
		return 0;
	}

	// Add some contension for BBQL ram
	if ((address >= KB(128)) && (address < KB(256))) {
		//m68k_modify_timeslice(-2);
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

	return SDL_SwapBE16(*(uint16_t *)&qlayMemSpace[address]);
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

	return SDL_SwapBE32(*(uint32_t *)&qlayMemSpace[address]);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	if ((address >= QL_INTERNAL_IO) &&
	    address < (QL_INTERNAL_IO +
		       QL_INTERNAL_IO_SIZE)) {
		qlHardwareWrite8(address, value);
		return;
	}

/*
	if ((address >= emulator::qlay_nfa_io) &&
	    address < (emulator::qlay_nfa_io + emulator::qlay_nfa_io_size)) {
		qldisk::wrnfa(address, value);
		emulator::q68MemorySpace[address] = value;
	}
*/

	if (address >= qlayMemSize) {
		return;
	}

	if ((address >= KB(128)) && (address < KB(256))) {
		//m68k_modify_timeslice(-6);
	}

	qlayMemSpace[address] = value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	m68k_modify_timeslice(-4);
	m68k_write_memory_8(address + 0, (value >> 8) & 0xFF);
	m68k_write_memory_8(address + 1, (value >> 0) & 0xFF);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	m68k_modify_timeslice(-12);
	m68k_write_memory_8(address + 0, (value >> 24) & 0xFF);
	m68k_write_memory_8(address + 1, (value >> 16) & 0xFF);
	m68k_write_memory_8(address + 2, (value >> 8) & 0xFF);
	m68k_write_memory_8(address + 3, (value >> 0) & 0xFF);
}
