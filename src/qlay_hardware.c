/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <sys/time.h>

#include "emulator_hardware.h"
#include "emulator_logging.h"
#include "emulator_options.h"
#include "emulator_screen.h"
#include "qlay_io.h"
#include "qlay_sound.h"
#include "spi_sdcard.h"

// ghost irq registers
Uint8 EMU_PC_INTR = 0;
Uint8 EMU_PC_INTR_MASK = 0;
Uint8 EMU_MC_STAT = 0;
Uint8 EMU_PC_TRAK1 = 0;
Uint8 EMU_PC_TRAK2 = 0;

// ghost RTC register
Uint32 EMU_PC_CLOCK = 0;

// qsound state
bool qsound_enabled = false;
Uint32 qsound_addr = 0;

void qlayInitialiseTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	EMU_PC_CLOCK = tv.tv_sec + QDOS_TIME;
}

void qlayInitialiseQsound(void)
{
	const char *qsound = emulatorOptionString("qsound");

	// no qsound set
	if (!qsound) {
		return;
	}

	qsound_addr = SDL_strtol(qsound, NULL, 16);

	if (qsound_addr != 0) {
		SDL_LogInfo(QLAY_LOG_HW, "QSound address 0x%X", qsound_addr);
		qsound_enabled = true;
	}
}

static Uint8 EMU_QLSD_SPI_SELECT = 0x00;
static Uint8 EMU_QLSD_MOSI = 0x00;
static Uint8 EMU_QLSD_CLK = 0x00;
static Uint8 EMU_QLSD_SPI_READ = 0x00;
static Uint8 QLSDCount = 0;
static Uint8 QLSDInCount = 0;
static Uint8 QLSDByteOut = 0x00;
static Uint8 QLSDByteIn = 0x00;
static bool QLSDEnabled = false;
static bool QLSDBG = false;

Uint8 qlHardwareRead8(unsigned int addr)
{
	switch (addr) {
	case PC_CLOCK: // 18000
		return EMU_PC_CLOCK >> 24;
	case PC_CLOCK + 1:
		return (EMU_PC_CLOCK >> 16) & 0xFF;
	case PC_CLOCK + 2:
		return (EMU_PC_CLOCK >> 8) & 0xFF;
	case PC_CLOCK + 3:
		return EMU_PC_CLOCK & 0xFF;
	case PC_IPCRD: // 18020
		return readQLHw(addr);
	case PC_INTR: // 18021
		return EMU_PC_INTR;
	case PC_TRAK1: // 18022
	case PC_TRAK2: // 18023
		return readQLHw(addr);
	case MC_STAT: // 18063
		return EMU_MC_STAT;
	default:
		break;
	}

	// QL-SD
	if (addr >= 0xFee0 && addr < 0x10000) {
		// enable the interface and return immediately
		if (addr == (QLSD_IF_BASE + QLSD_IF_ENABLE)) {
			QLSDEnabled = true;
			SDL_LogDebug(QLAY_LOG_HW, "QL-SD: enabled");
			return 0;
		}

		// Handle background transfers
		if (QLSDBG && ((addr >= QLSD_SPI_XFER) && (addr < 0x10000))) {
			card_byte_in(EMU_QLSD_SPI_SELECT - 1, addr & 0xFF);
			EMU_QLSD_SPI_READ =
				card_byte_out(EMU_QLSD_SPI_SELECT - 1);
			return 0;
		}

		if (QLSDEnabled) {
			switch (addr - QLSD_IF_BASE) {
			case QLSD_IF_DISABLE:
				QLSDEnabled = false;
				SDL_LogDebug(QLAY_LOG_HW, "QL-SD: disabled");
				break;
			case QLSD_IF_RESET:
				EMU_QLSD_SPI_SELECT = 0x00;
				EMU_QLSD_MOSI = 0x00;
				EMU_QLSD_CLK = 0x00;
				QLSDCount = 0;
				QLSDInCount = 0;
				QLSDByteIn = 0x00;
				QLSDByteOut = 0x00;
				QLSDBG = false;
				SDL_LogDebug(QLAY_LOG_HW, "QL-SD: reset");
				break;
			case QLSD_IF_VERSION:
				SDL_LogDebug(QLAY_LOG_HW, "QL-SD: version");
				break;
			case QLSD_SPI_READ:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: read");
				if (QLSDBG) {
					return EMU_QLSD_SPI_READ;
				} else {
					return (QLSDByteIn & 0x80) ? 0xFF :
								     0x00;
				}
			case QLSD_SPI_XFER_FAST:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: bg xfer fast");
				QLSDBG = true;
				break;
			case QLSD_SPI_XFER_SLOW:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: bg xfer slow");
				QLSDBG = true;
				break;
			case QLSD_SPI_XFER_OFF:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: bg xfer off");
				QLSDBG = false;
				break;
			case QLSD_SPI_SELECT0:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: deselect");
				EMU_QLSD_SPI_SELECT = 0;
				break;
			case QLSD_SPI_SELECT1:
				EMU_QLSD_SPI_SELECT = 1;
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: select1");
				break;
			case QLSD_SPI_SELECT2:
				EMU_QLSD_SPI_SELECT = 2;
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: select2");
				break;
			case QLSD_SPI_SELECT3:
				//we dont support card 3 yet
				//EMU_QLSD_SPI_SELECT = 3;
				EMU_QLSD_SPI_SELECT = 0;
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: select3");
				break;
			case QLSD_SPI_CLR_MOSI:
				EMU_QLSD_MOSI = 0;
				break;
			case QLSD_SPI_SET_MOSI:
				EMU_QLSD_MOSI = 1;
				break;
			case QLSD_SPI_CLR_SCLK:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: clk 0");
				if (EMU_QLSD_CLK && EMU_QLSD_SPI_SELECT) {
					if (QLSDInCount == 0) {
						QLSDByteIn = card_byte_out(
							EMU_QLSD_SPI_SELECT -
							1);
					} else {
						QLSDByteIn <<= 1;
					}
					QLSDInCount++;
					QLSDInCount %= 8;
				}
				EMU_QLSD_CLK = 0;
				break;
			case QLSD_SPI_SET_SCLK:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: clk 1");

				if (!EMU_QLSD_CLK && EMU_QLSD_SPI_SELECT) {
					QLSDByteOut <<= 1;
					if (EMU_QLSD_MOSI) {
						QLSDByteOut |= 1;
					}
					QLSDCount++;
					if (QLSDCount == 8) {
						card_byte_in(
							EMU_QLSD_SPI_SELECT - 1,
							QLSDByteOut);
						SDL_LogDebug(
							Q68_LOG_HW,
							"QL-SD: byte out %2.2X",
							QLSDByteOut);
						QLSDCount = 0;
					}
				}
				EMU_QLSD_CLK = 1;
				break;
			default:
				SDL_LogDebug(Q68_LOG_HW, "QL-SD: unknown %4.4x",
					     addr);
				break;
			}
		}
	}

	return 0;
}

void qlHardwareWrite8(unsigned int addr, Uint8 val)
{
	switch (addr) {
	case PC_CLOCK:
		EMU_PC_CLOCK = 0;
		return;
	case PC_CLOCK + 1:
		switch (val & 0x1E) {
		case 0x0E:
			EMU_PC_CLOCK += 0x01000000;
			break;
		case 0x16:
			EMU_PC_CLOCK += 0x00010000;
			break;
		case 0x1A:
			EMU_PC_CLOCK += 0x00000100;
			break;
		case 0x1C:
			EMU_PC_CLOCK += 0x00000001;
			break;
		}
		return;
	case PC_TCTRL: // 18002
		wrZX8302(val);
		return;
	case PC_IPCWR: // 18003
		wr8049(val);
		return;
	case PC_MCTRL: // 18020
		wrmdvcntl(val);
		return;
	case PC_INTR: // 18021
	{
		Uint8 tmp;
		EMU_PC_INTR_MASK = (val & 0xE0);
		tmp = val & 0x1F;
		EMU_PC_INTR &= ~tmp;
		return;
	}
	case PC_TRAK1: // 18022
		writeMdvSer(val);
		return;
	case MC_STAT: // 18063
		EMU_MC_STAT = val;
		if (val & MC__SCRN) {
			emulatorSecondScreen = true;
		} else {
			emulatorSecondScreen = false;
		}
		if (val & MC__M256) {
			emulatorScreenChangeMode(0);
		} else {
			emulatorScreenChangeMode(1);
		}
		return;
	default:
		break;
	}
}

enum { QSOUND_PA, QSOUND_CA, QSOUND_PB, QSOUND_CB };
#define QSOUND_REG_BIT (1 << 0)
#define QSOUND_LATCH_BIT (1 << 2)
static Uint8 qsound_pa_last;
static Uint8 qsound_pb_last;
static Uint8 qsound_reg_num;

void qsoundWrite(Uint32 address, Uint8 val)
{
	switch (address % 4) {
	case QSOUND_PA:
		SDL_LogDebug(QLAY_LOG_HW, "QSound PA data 0x%X", val);
		qsound_pa_last = val;
		break;
	case QSOUND_PB:
		SDL_LogDebug(QLAY_LOG_HW, "QSound PB data 0x%X", val);
		if ((qsound_pb_last & QSOUND_LATCH_BIT) &&
		    !(val & QSOUND_LATCH_BIT)) {
			if (qsound_pb_last & QSOUND_REG_BIT) {
				SDL_LogDebug(QLAY_LOG_HW,
					     "QSound Latching reg_num 0x%2.2X",
					     qsound_pa_last);
				qsound_reg_num = qsound_pa_last;
			} else {
				SDL_LogDebug(
					QLAY_LOG_HW,
					"QSound Latching reg_val 0x%2.2X 0x%2.2X",
					qsound_reg_num, qsound_pa_last);

				qlaySetAYRegister(qsound_reg_num,
						  qsound_pa_last);
			}
		}
		qsound_pb_last = val;
		break;
	default:
		SDL_LogDebug(QLAY_LOG_HW, "QSound control 0x%X", val);
	}
}
