/*
 * Copyright (c) 2025 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */
#include <SDL3/SDL.h>
#include <stdio.h>

#include "emulator_options.h"
#include "m68k.h"
#include "uthash.h"

struct trace_entry {
  int addr; /* key */
  char name[21];
  UT_hash_handle hh; /* makes this structure hashable */
};

static bool trace = false;
static Uint32 traceLow;
static Uint32 traceHigh;
static struct trace_entry* traceHash = NULL;

void emulatorTraceInit(void)
{
  const char* mapFile = emulatorOptionString("trace-map");
  FILE* file;
  char symbol[21];
  unsigned int addr;

  /* Initialise high/low */
  traceLow = emulatorOptionInt("trace-low");
  traceHigh = emulatorOptionInt("trace-high");

  SDL_Log("Tracing range %8.8x - %8.8x", traceLow, traceHigh);

  if (emulatorOptionInt("trace")) {
    trace = true;
    SDL_Log("Tracing Enabled");
  }

  if (SDL_strlen(mapFile) == 0) {
    return;
  }

  file = fopen(mapFile, "r");
  if (file == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
        "Could not open mapfile: %s", mapFile);
    return;
  }

  while (fscanf(file, "%x,%20s", &addr, symbol) == 2) {
    struct trace_entry* traceEntry;

    HASH_FIND_INT(traceHash, &addr, traceEntry);
    if (traceEntry != NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
          "Duplicate address %x,%s\n", addr, symbol);
    } else {
      traceEntry = SDL_malloc(sizeof(struct trace_entry));
      traceEntry->addr = addr;
      SDL_strlcpy(traceEntry->name, symbol,
          sizeof(traceEntry->name));
      HASH_ADD_INT(traceHash, addr, traceEntry);
    }
  }

  fclose(file);
}

static const char* traceSymbol(int addr)
{
  struct trace_entry* traceEntry;

  HASH_FIND_INT(traceHash, &addr, traceEntry);
  if (traceEntry != NULL) {
    return traceEntry->name;
  } else {
    return NULL;
  }
}

void emulatorTraceToggle(void)
{
  trace = !trace;
}

void emulatorTrace(void)
{
  if (!trace) {
    return;
  }

  Uint32 pc = m68k_get_reg(NULL, M68K_REG_PC);

  if ((pc >= traceLow) && (pc <= traceHigh)) {
    char disBuf[256];
    Uint32 d[8];
    Uint32 a[8];
    Uint32 sr;
    const char* symbol;
    int i;

    for (i = 0; i < 8; i++) {
      d[i] = m68k_get_reg(NULL, M68K_REG_D0 + i);
    }

    for (i = 0; i < 8; i++) {
      a[i] = m68k_get_reg(NULL, M68K_REG_A0 + i);
    }

    sr = m68k_get_reg(NULL, M68K_REG_SR) & 0x1F;

    m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);

    symbol = traceSymbol(pc);

    // Output the registers and assembly
    SDL_Log("  | %8c| %8c| %8c| %8c| %8c| %8c| %8c| %8c|", '0', '1',
        '2', '3', '4', '5', '6', '7');
    SDL_Log("D | %8x| %8x| %8x| %8x| %8x| %8x| %8x| %8x|", d[0],
        d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    SDL_Log("A | %8x| %8x| %8x| %8x| %8x| %8x| %8x| %8x|", a[0],
        a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    SDL_Log("S | X=%1d N=%1d Z=%1d V=%1d C=%1d", (sr >> 4) & 1,
        (sr >> 3) & 1, (sr >> 2) & 1, (sr >> 1) & 1, sr & 1);
    if (symbol) {
      SDL_Log("%s", symbol);
    }
    SDL_Log("%08X %s\n\n", pc, disBuf);
  }
}
