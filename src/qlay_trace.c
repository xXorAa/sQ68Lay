
/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <stdbool.h>
#include <stdio.h>

#include "emulator_options.h"
#include "uthash.h"

struct trace_entry {
	int addr;			/* key */
	char name[20];
	UT_hash_handle hh;		/* makes this structure hashable */
};

static bool traceMap = false;
static struct trace_entry *traceHash;

void traceInit(void)
{
	const char *mapFile = emulatorOptionString("trace-map");
	FILE *file;
	char symbol[21];
	unsigned int addr;

	if (strlen(mapFile) == 0) {
		return;
	}

	file = fopen(mapFile, "r");
	if (file == NULL) {
		fprintf(stderr, "Could not open mapfile: %s\n", mapFile);
		return;
	}

	while (fscanf(file, "%x,%20s", &addr, symbol) == 2) {
		struct trace_entry *traceEntry;

		HASH_FIND_INT(traceHash, &addr, traceEntry);
		if (traceEntry != NULL) {
			fprintf(stderr, "Duplicate address %x,%s\n", addr, symbol);
		} else {
			traceEntry = malloc(sizeof(struct trace_entry));
			traceEntry->addr = addr;
			strncpy(traceEntry->name, symbol, sizeof(traceEntry->name));
			HASH_ADD_INT(traceHash, addr, traceEntry);
		}
	}

	fclose(file);

	traceMap = true;
}

bool traceEnabled(void)
{
	return traceMap;
}

const char *traceSymbol(int addr)
{
	struct trace_entry *traceEntry;

	HASH_FIND_INT(traceHash, &addr, traceEntry);
	if (traceEntry != NULL) {
		return traceEntry->name;
	} else {
		return NULL;
	}
}

