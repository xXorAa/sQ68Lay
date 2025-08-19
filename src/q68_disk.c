/*
 * Copyright (c) 2025 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>

#include "emulator_logging.h"
#include "emulator_memory.h"
#include "emulator_options.h"
#include "ff.h"
#include "diskio.h"
#include "spi_sdcard.h"

SDL_IOStream *sd1Stream = NULL;
SDL_IOStream *sd2Stream = NULL;

bool q68DiskInitialise(void)
{
	const char *diskName = emulatorOptionString("sd1");
	if (!diskName || (strlen(diskName) == 0)) {
		SDL_LogDebug(Q68_LOG_DISK, "No SD card specified for SD1");
	} else {
		sd1Stream = SDL_IOFromFile(diskName, "r+b");
		if (sd1Stream == NULL) {
			SDL_LogError(Q68_LOG_DISK,
				     "Failed to open SD1 image: %s %s",
				     diskName, SDL_GetError());
			return false;
		}
	}

	diskName = emulatorOptionString("sd2");
	if (!diskName || (strlen(diskName) == 0)) {
		SDL_LogDebug(Q68_LOG_DISK, "No SD card specified for SD2");
	} else {
		sd2Stream = SDL_IOFromFile(diskName, "r+b");
		if (sd2Stream == NULL) {
			SDL_LogError(Q68_LOG_DISK,
				     "Failed to open SD2 card: %s %s", diskName,
				     SDL_GetError());
			return false;
		}
	}

	card_initialise(sd1Stream, sd2Stream);

	return true;
}

Uint32 q68DiskReadSMSQE(void)
{
	FRESULT res;
	FATFS fs;
	FIL fp;
	Uint32 ret = Q68_INVALID;

	res = f_mount(&fs, "0", 0);
	if (res != FR_OK) {
		SDL_LogDebug(Q68_LOG_DISK, "Failed to mount FATFS: %d", res);
		return Q68_INVALID;
	}

	res = f_open(&fp, "Q68_ROM.SYS", FA_READ);
	if (res != FR_OK) {
		SDL_LogDebug(Q68_LOG_DISK,
			     "Failed to open Q68_ROM.SYS file: %d",
			     f_error(&fp));
	} else {
		FSIZE_t size = f_size(&fp);
		UINT br;
		res = f_read(&fp, &emulatorMemorySpace()[Q68_SYSROM_ADDR], size,
			     &br);

		f_close(&fp);

		ret = Q68_SYSROM_ADDR;
		goto out;
	}

	res = f_open(&fp, "Q68_SMSQ.SYS", FA_READ);
	if (res != FR_OK) {
		SDL_LogDebug(Q68_LOG_DISK,
			     "Failed to open Q68_SMSQ.SYS file: %d",
			     f_error(&fp));
	} else {
		FSIZE_t size = f_size(&fp);

		UINT br;
		res = f_read(&fp, &emulatorMemorySpace()[Q68_SMSQE_RAM_ADDR],
			     size, &br);

		f_close(&fp);

		ret = Q68_SMSQE_WIN_ADDR;
		goto out;
	}

	res = f_open(&fp, "Q68_SMSQ.WIN", FA_READ);
	if (res != FR_OK) {
		SDL_LogDebug(Q68_LOG_DISK,
			     "Failed to open Q68_SMSQ.WIN file: %d",
			     f_error(&fp));
	} else {
		FSIZE_t size = f_size(&fp);

		// the SMSQE file is at 0x1040 inside the WIN file
		// as the files are contiguous we can make use of that
		// without parsing win container
		UINT br;
		res = f_read(&fp,
			     &emulatorMemorySpace()[Q68_SMSQE_WIN_ADDR -
						    Q68_SMSQE_WIN_OFFSIZE],
			     size, &br);

		f_close(&fp);

		ret = Q68_SMSQE_WIN_ADDR;
		goto out;
	}

	ret = Q68_INVALID;
out:
	f_unmount("0");
	return ret;
}

DSTATUS disk_status(BYTE pdrv /* [IN] Physical drive number */
)
{
	(void)pdrv;
	return RES_OK;
}

DSTATUS disk_initialize(BYTE pdrv /* [IN] Physical drive number */
)
{
	(void)pdrv;
	return RES_OK;
}

DRESULT disk_read(BYTE pdrv, /* [IN] Physical drive number */
		  BYTE *buff, /* [OUT] Pointer to the read data buffer */
		  LBA_t sector, /* [IN] Start sector number */
		  UINT count /* [IN] Number of sectros to read */
)
{
	(void)pdrv; // Unused parameter

	SDL_LogDebug(Q68_LOG_DISK,
		     "disk_read: pdrv=%d, sector=%" PRIu32 ", count=%u", pdrv,
		     sector, count);

	Sint64 resSeek = SDL_SeekIO(sd1Stream, sector * 512, SDL_IO_SEEK_SET);
	if (resSeek < 0) {
		SDL_LogError(Q68_LOG_DISK, "Failed to seek SD1: %s",
			     SDL_GetError());
		return RES_ERROR;
	}

	size_t resRead = SDL_ReadIO(sd1Stream, buff, count * 512);
	if (resRead != count * 512) {
		SDL_LogError(Q68_LOG_DISK, "Failed to read SD1: %s",
			     SDL_GetError());
		return RES_ERROR;
	}

	return RES_OK;
}
