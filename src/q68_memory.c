/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "emulator_hardware.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "m68k.h"

static uint8_t* q68MemorySpace = NULL;
static uint8_t* q68ScreenSpace = NULL;
bool romProtect = false;

uint8_t* emulatorMemorySpace(void)
{
  return q68MemorySpace;
}

uint8_t* emulatorScreenSpace(void)
{
  return q68ScreenSpace;
}

int emulatorInitMemory(void)
{
  q68MemorySpace = calloc(Q68_RAM_SIZE, 1);
  q68ScreenSpace = calloc(Q68_SCREEN_SIZE, 1);

  return 0;
}

unsigned int m68k_read_memory_8(unsigned int address)
{
  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    return qlHardwareRead8(address);
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    return qlHardwareRead8(address);
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    return q68ScreenSpace[address - Q68_SCREEN];
  }

  if (address >= Q68_Q40_IO) {
    return qlHardwareRead8(address);
  }

  if (address >= Q68_RAM_SIZE) {
    return 0;
  }

  return q68MemorySpace[address];
}

unsigned int m68k_read_memory_16(unsigned int address)
{
  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    return ((uint16_t)qlHardwareRead8(address) << 8) | qlHardwareRead8(address + 1);
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    return ((uint16_t)qlHardwareRead8(address) << 8) | qlHardwareRead8(address + 1);
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    return SDL_Swap16BE(
        *(uint16_t*)&q68ScreenSpace[address - Q68_SCREEN]);
  }

  if (address >= Q68_Q40_IO) {
    return ((uint16_t)qlHardwareRead8(address) << 8) | qlHardwareRead8(address + 1);
  }

  if (address >= Q68_RAM_SIZE) {
    return 0;
  }

  return SDL_Swap16BE(*(uint16_t*)&q68MemorySpace[address]);
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
  if (address >= Q68_RAM_SIZE) {
    return 0;
  }

  return SDL_Swap16BE(*(uint16_t*)&q68MemorySpace[address]);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    return ((uint32_t)qlHardwareRead8(address) << 24) | ((uint32_t)qlHardwareRead8(address + 1) << 16) | ((uint32_t)qlHardwareRead8(address + 2) << 8) | ((uint32_t)qlHardwareRead8(address + 3) << 0);
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    return ((uint32_t)qlHardwareRead8(address) << 24) | ((uint32_t)qlHardwareRead8(address + 1) << 16) | ((uint32_t)qlHardwareRead8(address + 2) << 8) | ((uint32_t)qlHardwareRead8(address + 3) << 0);
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    return SDL_Swap32BE(
        *(uint32_t*)&q68ScreenSpace[address - Q68_SCREEN]);
  }

  if (address >= Q68_Q40_IO) {
    return ((uint32_t)qlHardwareRead8(address) << 24) | ((uint32_t)qlHardwareRead8(address + 1) << 16) | ((uint32_t)qlHardwareRead8(address + 2) << 8) | ((uint32_t)qlHardwareRead8(address + 3) << 0);
  }

  if (address >= Q68_RAM_SIZE) {
    return 0;
  }

  return SDL_Swap32BE(*(uint32_t*)&q68MemorySpace[address]);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
  if (address >= Q68_RAM_SIZE) {
    return 0;
  }

  return SDL_Swap32BE(*(uint32_t*)&q68MemorySpace[address]);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
  if (romProtect && (address <= Q68_ROM_SIZE)) {
    return;
  }

  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value);
    return;
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value);
    return;
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    emulatorScreenSpace()[address - Q68_SCREEN] = value;
    return;
  }

  if (address >= Q68_Q40_IO) {
    qlHardwareWrite8(address, value);
    return;
  }

  if (address >= Q68_RAM_SIZE) {
    return;
  }

  emulatorMemorySpace()[address] = value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
  if (romProtect && (address <= Q68_ROM_SIZE)) {
    return;
  }

  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value >> 8);
    qlHardwareWrite8(address + 1, value & 0xFF);
    return;
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value >> 8);
    qlHardwareWrite8(address + 1, value & 0xFF);
    return;
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    *(uint16_t*)&q68ScreenSpace[address - Q68_SCREEN] = SDL_Swap16BE(value);
    return;
  }

  if (address >= Q68_Q40_IO) {
    qlHardwareWrite8(address, value >> 8);
    qlHardwareWrite8(address + 1, value & 0xFF);
    return;
  }

  if (address >= Q68_RAM_SIZE) {
    return;
  }

  *(uint16_t*)&q68MemorySpace[address] = SDL_Swap16BE(value);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
  if (romProtect && (address <= Q68_ROM_SIZE)) {
    return;
  }

  if ((address >= QL_INTERNAL_IO) && address < (QL_INTERNAL_IO + QL_INTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value >> 24);
    qlHardwareWrite8(address, (value >> 16) & 0xFF);
    qlHardwareWrite8(address, (value >> 8) & 0xFF);
    qlHardwareWrite8(address + 3, value & 0xFF);
    return;
  }

  if ((address >= QL_EXTERNAL_IO) && address < (QL_EXTERNAL_IO + QL_EXTERNAL_IO_SIZE)) {
    qlHardwareWrite8(address, value >> 24);
    qlHardwareWrite8(address, (value >> 16) & 0xFF);
    qlHardwareWrite8(address, (value >> 8) & 0xFF);
    qlHardwareWrite8(address + 3, value & 0xFF);
    return;
  }

  if ((address >= Q68_SCREEN) && address < (Q68_SCREEN + Q68_SCREEN_SIZE)) {
    *(uint32_t*)&q68ScreenSpace[address - Q68_SCREEN] = SDL_Swap32BE(value);
    return;
  }

  if (address >= Q68_Q40_IO) {
    qlHardwareWrite8(address, value >> 24);
    qlHardwareWrite8(address, (value >> 16) & 0xFF);
    qlHardwareWrite8(address, (value >> 8) & 0xFF);
    qlHardwareWrite8(address + 3, value & 0xFF);
    return;
  }

  if (address >= Q68_RAM_SIZE) {
    return;
  }

  *(uint32_t*)&q68MemorySpace[address] = SDL_Swap32BE(value);
}
