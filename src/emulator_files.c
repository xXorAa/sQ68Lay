/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <fcntl.h>
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
	size_t fileSize;
	int file;

	//if (name[0] == '~') {
	//    name.erase(0, 1);
	//    name.insert(0, "/");
	//    name.insert(0, homedir);
	//}

	if (!emulatorFileExists(name)) {
		fprintf(stderr, "File Not Found %s\n", name);
		return;
	}

	fileSize = emulatorFileSize(name);

	if (wantSize != 0) {
		if (fileSize != wantSize) {
			fprintf(stderr, "File Size Mismatch %s %zu != %zu",
				name, wantSize, fileSize);
			return;
		}
	}

	file = open(name, O_RDONLY | O_BINARY);
	read(file, addr, fileSize);
	close(file);
}
