/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

bool emulatorFileExists(const char* name)
{
	struct stat statBuf;
	int ret;

	ret = stat(name, &statBuf);
	if (ret < 0) {
		return false;
	}
	return true;
}

bool emulatorIsDirectory(const char* name)
{
	struct stat statBuf;
	int ret;

	ret = stat(name, &statBuf);
	if (ret < 0) {
		return false;
	}
	return S_ISDIR(statBuf.st_mode);
}

size_t emulatorFileSize(const char* name)
{
	struct stat statBuf;
	int ret;

	ret = stat(name, &statBuf);
	if (ret < 0) {
		return 0;
	}
	return statBuf.st_size;
}

void emulatorLoadFile(const char* name, uint8_t* addr, size_t wantSize)
{
	gchar *contents;
	gsize length;
	GError *error = NULL;

	g_file_get_contents (name, &contents, &length, &error);
	if (error != NULL) {
		fprintf (stderr, "Unable to read file: %s\n", error->message);
		g_error_free (error);
		return;
	}

	if (wantSize != 0) {
		if (length != wantSize) {
			fprintf(stderr, "File Size Mismatch %s %zu != %zu",
				name, wantSize, length);
			g_free(contents);
			g_error_free(error);
			return;
		}
	}

	memcpy(addr, contents, length);
	g_free(contents);
}
