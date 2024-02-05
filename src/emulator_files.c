/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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
		perror("What Happened");
		printf("GOT HERE\n");
		return 0;
	}

	return statBuf.st_size;
}

void emulatorLoadFile(const char* name, uint8_t* addr, size_t wantSize)
{
	size_t fileSize;
	int fd;

	if (!emulatorFileExists(name)) {
		fprintf(stderr, "File Not Found %s\n", name);
		return;
	}

	fileSize = emulatorFileSize(name);
	if (wantSize && (fileSize != wantSize)) {
		fprintf(stderr, "File Size Mismatch %s %zu != %zu\n",
			name, wantSize, fileSize);
		return;
	}

	fd = open(name, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error opening file %s %s\n", name, strerror(errno));
		return;
	}

	read(fd, addr, fileSize);

	close(fd);
}
