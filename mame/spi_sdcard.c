// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
    SD Card emulation, SPI interface.
    Emulation by R. Belmont

    This emulates either an SDHC (SPI_SDCARD) or an SDV2 card (SPI_SDCARDV2).  SDHC has a fixed
    512 byte block size and the arguments to the read/write commands are block numbers.  SDV2
    has a variable block size defaulting to 512 and the arguments to the read/write commands
    are byte offsets.

    The block size set with CMD16 must match the underlying CHD block size if it's not 512.

    Adding the native 4-bit-wide SD interface is also possible; this should be broken up into a base
    SD Card class with SPI and SD frontends in that case.

    Multiple block read/write commands are not supported but would be straightforward to add.

    References:
    https://www.sdcard.org/downloads/pls/ (Physical Layer Simplified Specification)
    REF: tags are referring to the spec form above. 'Physical Layer Simplified Specification v8.00'

    http://www.dejazzer.com/ee379/lecture_notes/lec12_sd_card.pdf
    https://embdev.net/attachment/39390/TOSHIBA_SD_Card_Specification.pdf
    http://elm-chan.org/docs/mmc/mmc_e.html
*/

#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crc16spi_fujitsu.h"
#include "emulator_logging.h"
#include "spi_sdcard.h"

static const uint8_t DATA_RESPONSE_OK = 0x05;
static const uint8_t DATA_RESPONSE_IO_ERROR = 0x0d;
static const int SPI_DELAY_RESPONSE = 1;

// Handle non Windows
#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct {
	sd_type m_type;
	sd_state m_state;
	uint8_t m_data[520], m_cmd[6];
	int m_harddisk;

	int m_ss, m_in_bit, m_clk_state, m_;
	uint8_t m_in_latch, m_out_latch, m_cur_bit;
	uint16_t m_out_count, m_out_ptr, m_write_ptr, m_blksize;
	uint32_t m_blknext;
	bool m_bACMD;
} card;

card cards[2];

void card_initialise(const char *sd1, const char *sd2)
{
	SDL_LogDebug(Q68_LOG_SD, "SD Card Initialising");
	if (sd1 && strlen(sd1)) {
		int res = open(sd1, O_RDWR | O_BINARY);
		if (res >= 0) {
			SDL_LogDebug(Q68_LOG_SD, "SD1: %s", sd1);
			cards[0].m_type = SD_TYPE_HC;
			cards[0].m_harddisk = res;
			cards[0].m_blksize = 512;
		} else {
			SDL_LogError(Q68_LOG_SD, "SD1: failed to open %s - %s",
				     sd1, strerror(errno));
			cards[0].m_harddisk = -1;
		}
	} else {
		cards[0].m_harddisk = -1;
	}

	if (sd2 && strlen(sd2)) {
		int res = open(sd2, O_RDWR | O_BINARY);
		if (res >= 0) {
			SDL_LogDebug(Q68_LOG_SD, "SD1: %s", sd1);
			cards[1].m_type = SD_TYPE_HC;
			cards[1].m_harddisk = res;
			cards[1].m_blksize = 512;
		} else {
			SDL_LogError(Q68_LOG_SD, "SD2: failed to open %s - %s",
				     sd2, strerror(errno));
			cards[1].m_harddisk = -1;
		}
	} else {
		cards[1].m_harddisk = -1;
	}
}

static bool card_seek(int cardno, uint32_t blknext)
{
	off_t seekPos = (off_t)cards[cardno].m_blksize * (off_t)blknext;

	off_t resSeek = lseek(cards[cardno].m_harddisk, seekPos, SEEK_SET);
	if (resSeek < 0) {
		SDL_LogDebug(Q68_LOG_SD, "SD%.1d: failed to seek %" PRIdMAX,
			     cardno, (intmax_t)seekPos);

		return false;
	}

	return true;
}

static bool card_write(int cardno, uint32_t blknext, void *data)
{
	card_seek(cardno, blknext);

	ssize_t resWrite =
		write(cards[cardno].m_harddisk, data, cards[cardno].m_blksize);
	if (resWrite != cards[cardno].m_blksize) {
		SDL_LogError(Q68_LOG_SD, "SD%.1d: failed to write %" PRIdMAX,
			     cardno, (intmax_t)resWrite);

		return 0;
	}

	return 1;
}

static bool card_read(int cardno, uint32_t blknext, void *data)
{
	card_seek(cardno, blknext);

	ssize_t resRead =
		read(cards[cardno].m_harddisk, data, cards[cardno].m_blksize);
	if (resRead != cards[cardno].m_blksize) {
		SDL_LogError(Q68_LOG_SD,
			     "SD%.1d: failed to read =  %" PRIdMAX
			     ", blk = %" PRIu32,
			     cardno, (intmax_t)resRead, blknext);
		return false;
	}

	return true;
}

/*
void spi_ss_w(int state)
{
	m_ss = state;
}

void spi_mosi_w(int state)
{
	m_in_bit = state;
}

bool get_card_present(void) {
	return !(m_harddisk == NULL);
}
*/
void send_data(int cardno, uint16_t count, sd_state new_state)
{
	cards[cardno].m_out_ptr = 0;
	cards[cardno].m_out_count = count;
	change_state(cardno, new_state);
}

/*
void spi_clock_w(int state)
{
	// only respond if selected, and a clock edge
	if (m_ss && state != m_clk_state)
	{
		// We implement SPI Mode 3 signalling, in which we latch the data on
		// rising clock edges, and shift the data on falling clock edges.
		// See http://www.dejazzer.com/ee379/lecture_notes/lec12_sd_card.pdf for details
		// on the 4 SPI signalling modes. SD Cards can work in either Mode 0 or Mode 3,
		// both of which shift on the falling edge and latch on the rising edge but
		// have opposite CLK polarity.
		if (state)
			latch_in();
		else
			shift_out();
	}
	m_clk_state = state;
}

void latch_in(void)
{
	m_in_latch &= ~0x01;
	m_in_latch |= m_in_bit;
	printf("\tsdcard: L %02x (%d) (out %02x)\n", m_in_latch, m_cur_bit, m_out_latch);
	m_cur_bit++;
	if (m_cur_bit == 8)
	{
	}
}
*/

void card_byte_in(int cardno, uint8_t m_in_latch)
{
	SDL_LogDebug(Q68_LOG_SD, "SD%.1d: m_in_latch %2.2x", cardno,
		     m_in_latch);

	if (cards[cardno].m_harddisk < 0) {
		return;
	}

	for (uint8_t i = 0; i < 5; i++) {
		cards[cardno].m_cmd[i] = cards[cardno].m_cmd[i + 1];
	}
	cards[cardno].m_cmd[5] = m_in_latch;

	switch (cards[cardno].m_state) {
	case SD_STATE_IDLE:
		do_command(cardno);
		break;

	case SD_STATE_WRITE_WAITFE:
		if (m_in_latch == 0xfe) {
			cards[cardno].m_state = SD_STATE_WRITE_DATA;
			cards[cardno].m_out_latch = 0xff;
			cards[cardno].m_write_ptr = 0;
		}
		break;

	case SD_STATE_WRITE_DATA:
		cards[cardno].m_data[cards[cardno].m_write_ptr++] = m_in_latch;
		if (cards[cardno].m_write_ptr ==
		    (cards[cardno].m_blksize + 2)) {
			SDL_LogDebug(Q68_LOG_SD,
				     "writing LBA %x, data %02x %02x %02x %02x",
				     cards[cardno].m_blknext,
				     cards[cardno].m_data[0],
				     cards[cardno].m_data[1],
				     cards[cardno].m_data[2],
				     cards[cardno].m_data[3]);
			if (card_write(cardno, cards[cardno].m_blknext,
				       &cards[cardno].m_data[0])) {
				cards[cardno].m_data[0] = DATA_RESPONSE_OK;
			} else {
				cards[cardno].m_data[0] =
					DATA_RESPONSE_IO_ERROR;
			}
			cards[cardno].m_data[1] = 0x01;

			send_data(cardno, 2, SD_STATE_IDLE);
		}
		break;

	case SD_STATE_DATA_MULTI:
		do_command(cardno);
		if (cards[cardno].m_state == SD_STATE_DATA_MULTI &&
		    cards[cardno].m_out_count == 0) {
			cards[cardno].m_data[0] = 0xfe; // data token
			card_read(cardno, cards[cardno].m_blknext++,
				  &cards[cardno].m_data[1]);
			uint16_t crc16 = crc16spi_fujitsu_byte(
				0, &cards[cardno].m_data[1],
				cards[cardno].m_blksize);
			cards[cardno].m_data[cards[cardno].m_blksize + 1] =
				(crc16 >> 8) & 0xff;
			cards[cardno].m_data[cards[cardno].m_blksize + 2] =
				(crc16 & 0xff);
			send_data(cardno, 1 + cards[cardno].m_blksize + 2,
				  SD_STATE_DATA_MULTI);
		}
		break;

	default:
		if (((cards[cardno].m_cmd[0] & 0x70) == 0x40) ||
		    (cards[cardno].m_out_count == 0)) // CMD0 - GO_IDLE_STATE
		{
			do_command(cardno);
		}
		break;
	}
}

uint8_t card_byte_out(int cardno)
{
	if (cards[cardno].m_harddisk < 0) {
		return 0xff;
	}

	if (cards[cardno].m_out_ptr < SPI_DELAY_RESPONSE) {
		cards[cardno].m_out_ptr++;
	} else if (cards[cardno].m_out_count > 0) {
		cards[cardno].m_out_latch =
			cards[cardno].m_data[cards[cardno].m_out_ptr -
					     SPI_DELAY_RESPONSE];
		cards[cardno].m_out_ptr++;
		cards[cardno].m_out_count--;
	} else {
		cards[cardno].m_out_latch = 0xFF;
	}

	SDL_LogDebug(Q68_LOG_SD, "SD%.1d: m_out_latch %2.2x", cardno,
		     cards[cardno].m_out_latch);

	return cards[cardno].m_out_latch;
}

/*
void shift_out(void)
{
	m_in_latch <<= 1;
	m_out_latch <<= 1;
	m_out_latch |= 1;
	printf("\tsdcard: S %02x %02x (%d)\n", m_in_latch, m_out_latch, m_cur_bit);

	m_cur_bit &= 0x07;
	if (m_cur_bit == 0)
	{
		if (m_out_ptr < SPI_DELAY_RESPONSE)
		{
			m_out_ptr++;
		}
		else if (m_out_count > 0)
		{
			m_out_latch = m_data[m_out_ptr - SPI_DELAY_RESPONSE];
			m_out_ptr++;
			printf("SDCARD: latching %02x (start of shift)\n", m_out_latch);
			m_out_count--;
		}
	}
	//write_miso(BIT(m_out_latch, 7));
}
*/

void do_command(int cardno)
{
	if (((cards[cardno].m_cmd[0] & 0xc0) == 0x40) &&
	    (cards[cardno].m_cmd[5] & 1)) {
		SDL_LogDebug(Q68_LOG_SD,
			     "SD%.1d: cmd %02d %02x %02x %02x %02x %02x",
			     cardno, cards[cardno].m_cmd[0] & 0x3f,
			     cards[cardno].m_cmd[1], cards[cardno].m_cmd[2],
			     cards[cardno].m_cmd[3], cards[cardno].m_cmd[4],
			     cards[cardno].m_cmd[5]);
		bool clean_cmd = true;
		cards[cardno].m_out_latch = 0xFF;
		switch (cards[cardno].m_cmd[0] & 0x3f) {
		case 0: // CMD0 - GO_IDLE_STATE
			if (cards[cardno].m_harddisk >= 0) {
				cards[cardno].m_data[0] = 0x01;
				send_data(cardno, 1, SD_STATE_IDLE);
			} else {
				cards[cardno].m_data[0] = 0x00;
				send_data(cardno, 1, SD_STATE_INA);
			}
			break;

		case 1: // CMD1 - SEND_OP_COND
			cards[cardno].m_data[0] = 0x00;
			send_data(cardno, 1, SD_STATE_READY);
			break;

		case 8: // CMD8 - SEND_IF_COND (SD v2 only)
			cards[cardno].m_data[0] = 0x01;
			cards[cardno].m_data[1] = 0;
			cards[cardno].m_data[2] = 0;
			cards[cardno].m_data[3] = 0x01;
			cards[cardno].m_data[4] = 0xaa;
			send_data(cardno, 5, SD_STATE_IDLE);
			break;

		case 9: // CMD9 - SEND_CSD
			cards[cardno].m_data[0] = 0x00; // TODO
			send_data(cardno, 1, SD_STATE_STBY);
			break;

		case 10: // CMD10 - SEND_CID
			cards[cardno].m_data[0] = 0x00; // initial R1 response
			cards[cardno].m_data[1] =
				0xff; // throwaway byte before data transfer
			cards[cardno].m_data[2] = 0xfe; // data token
			cards[cardno].m_data[3] =
				'M'; // Manufacturer ID - we'll use M for MAME
			cards[cardno].m_data[4] =
				'M'; // OEM ID - MD for MAMEdev
			cards[cardno].m_data[5] = 'D';
			cards[cardno].m_data[6] = 'M'; // Product Name - "MCARD"
			cards[cardno].m_data[7] = 'C';
			cards[cardno].m_data[8] = 'A';
			cards[cardno].m_data[9] = 'R';
			cards[cardno].m_data[10] = 'D';
			cards[cardno].m_data[11] =
				0x10; // Product Revision in BCD (1.0)
			{
				uint32_t uSerial = 0x12345678;
				cards[cardno].m_data[12] =
					(uSerial >> 24) &
					0xff; // PSN - Product Serial Number
				cards[cardno].m_data[13] = (uSerial >> 16) &
							   0xff;
				cards[cardno].m_data[14] = (uSerial >> 8) &
							   0xff;
				cards[cardno].m_data[15] = (uSerial & 0xff);
			}
			cards[cardno].m_data[16] =
				0x01; // MDT - Manufacturing Date
			cards[cardno].m_data[17] =
				0x59; // 0x15 9 = 2021, September
			cards[cardno].m_data[18] =
				0x00; // CRC7, bit 0 is always 0
			{
				uint16_t crc16 = crc16spi_fujitsu_byte(
					0, &cards[cardno].m_data[3], 16);
				cards[cardno].m_data[19] = (crc16 >> 8) & 0xff;
				cards[cardno].m_data[20] = (crc16 & 0xff);
			}
			send_data(cardno, 3 + 16 + 2, SD_STATE_STBY);
			break;

		case 12: // CMD12 - STOP_TRANSMISSION
			cards[cardno].m_data[0] = 0;
			send_data(cardno, 1,
				  cards[cardno].m_state == SD_STATE_RCV ?
					  SD_STATE_PRG :
					  SD_STATE_TRAN);
			break;

		case 13: // CMD13 - SEND_STATUS
			cards[cardno].m_data[0] = 0; // TODO
			send_data(cardno, 1, SD_STATE_STBY);
			break;

		case 16: // CMD16 - SET_BLOCKLEN
			cards[cardno].m_blksize =
				((uint16_t)cards[cardno].m_cmd[3] << 8) |
				(uint16_t)cards[cardno].m_cmd[4];
			if (cards[cardno].m_harddisk >= 0) {
				cards[cardno].m_data[0] = 0;
			} else {
				cards[cardno].m_data[0] =
					0xff; // indicate an error
				// if false was returned, it means the hard disk is a CHD file, and we can't resize the
				// blocks on CHD files.
				SDL_LogError(
					Q68_LOG_SD,
					"spi_sdcard: Couldn't change block size to %d, wrong CHD file?",
					cards[cardno].m_blksize);
			}
			send_data(cardno, 1, SD_STATE_TRAN);
			break;

		case 17: // CMD17 - READ_SINGLE_BLOCK
			if (cards[cardno].m_harddisk >= 0) {
				cards[cardno].m_data[0] =
					0x00; // initial R1 response
				// data token occurs some time after the R1 response.  A2SD expects at least 1
				// byte of space between R1 and the data packet.
				cards[cardno].m_data[1] = 0xff;
				cards[cardno].m_data[2] = 0xfe; // data token
				uint32_t blk = ((uint32_t)cards[cardno].m_cmd[1]
						<< 24) |
					       ((uint32_t)cards[cardno].m_cmd[2]
						<< 16) |
					       ((uint32_t)cards[cardno].m_cmd[3]
						<< 8) |
					       (uint32_t)cards[cardno].m_cmd[4];
				if (cards[cardno].m_type == SD_TYPE_V2) {
					blk /= cards[cardno].m_blksize;
				}
				card_read(cardno, blk,
					  &cards[cardno].m_data[3]);
				{
					uint16_t crc16 = crc16spi_fujitsu_byte(
						0, &cards[cardno].m_data[3],
						cards[cardno].m_blksize);
					cards[cardno]
						.m_data[cards[cardno].m_blksize +
							3] = (crc16 >> 8) &
							     0xff;
					cards[cardno]
						.m_data[cards[cardno].m_blksize +
							4] = (crc16 & 0xff);
				}
				send_data(cardno,
					  3 + cards[cardno].m_blksize + 2,
					  SD_STATE_DATA);
			} else {
				cards[cardno].m_data[0] = 0xff; // show an error
				send_data(cardno, 1, SD_STATE_DATA);
			}
			break;

		case 18: // CMD18 - CMD_READ_MULTIPLE_BLOCK
			if (cards[cardno].m_harddisk >= 0) {
				cards[cardno].m_data[0] =
					cards[cardno].m_out_latch =
						0x00; // initial R1 response
				// data token occurs some time after the R1 response.  A2SD
				// expects at least 1 byte of space between R1 and the data
				// packet.
				cards[cardno].m_blknext =
					((uint32_t)cards[cardno].m_cmd[1]
					 << 24) |
					((uint32_t)cards[cardno].m_cmd[2]
					 << 16) |
					((uint32_t)cards[cardno].m_cmd[3]
					 << 8) |
					(uint32_t)cards[cardno].m_cmd[4];
				if (cards[cardno].m_type == SD_TYPE_V2) {
					cards[cardno].m_blknext /=
						cards[cardno].m_blksize;
				}
			} else {
				cards[cardno].m_data[0] = 0xff; // show an error
			}
			send_data(cardno, 1, SD_STATE_DATA_MULTI);
			break;

		case 24: // CMD24 - WRITE_BLOCK
			cards[cardno].m_data[0] = 0;
			cards[cardno].m_blknext =
				((uint32_t)cards[cardno].m_cmd[1] << 24) |
				((uint32_t)cards[cardno].m_cmd[2] << 16) |
				((uint32_t)cards[cardno].m_cmd[3] << 8) |
				(uint32_t)cards[cardno].m_cmd[4];
			if (cards[cardno].m_type == SD_TYPE_V2) {
				cards[cardno].m_blknext /=
					cards[cardno].m_blksize;
			}
			send_data(cardno, 1, SD_STATE_WRITE_WAITFE);
			break;

		case 41:
			if (cards[cardno].m_bACMD) // ACMD41 - SD_SEND_OP_COND
			{
				cards[cardno].m_data[0] = 0;
				send_data(cardno, 1,
					  SD_STATE_READY); // + SD_STATE_IDLE
			} else // CMD41 - illegal
			{
				cards[cardno].m_data[0] = 0xff;
				send_data(cardno, 1, SD_STATE_INA);
			}
			break;

		case 55: // CMD55 - APP_CMD
			cards[cardno].m_data[0] = 0x01;
			send_data(cardno, 1, SD_STATE_IDLE);
			break;

		case 58: // CMD58 - READ_OCR
			cards[cardno].m_data[0] = 0;
			if (cards[cardno].m_type == SD_TYPE_HC) {
				cards[cardno].m_data[1] =
					0x40; // indicate SDHC support
			} else {
				cards[cardno].m_data[1] = 0;
			}
			cards[cardno].m_data[2] = 0;
			cards[cardno].m_data[3] = 0;
			cards[cardno].m_data[4] = 0;
			send_data(cardno, 5, SD_STATE_DATA);
			break;

		case 59: // CMD59 - CRC_ON_OFF
			cards[cardno].m_data[0] = 0;
			// TODO CRC 1-on, 0-off
			send_data(cardno, 1, SD_STATE_STBY);
			break;

		default:
			SDL_LogError(Q68_LOG_SD, "SD%.1d: Unsupported %02x\n",
				     cardno, cards[cardno].m_cmd[0] & 0x3f);
			clean_cmd = false;
			break;
		}

		// if this is command 55, that's a prefix indicating the next command is an "app command" or "ACMD"
		if ((cards[cardno].m_cmd[0] & 0x3f) == 55) {
			cards[cardno].m_bACMD = true;
		} else {
			cards[cardno].m_bACMD = false;
		}

		if (clean_cmd) {
			for (uint8_t i = 0; i < 6; i++) {
				cards[cardno].m_cmd[i] = 0xff;
			}
		}
	}
}

void change_state(int cardno, sd_state new_state)
{
	// TODO validate if transition is valid using refs below.
	// REF Figure 4-13:SD Memory Card State Diagram (Transition Mode)
	// REF Table 4-35:Card State Transition Table
	cards[cardno].m_state = new_state;
}
