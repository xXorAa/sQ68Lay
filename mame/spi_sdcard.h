// license:BSD-3-Clause
// copyright-holders:R. Belmont
#ifndef MAME_MACHINE_SPI_SDCARD_H
#define MAME_MACHINE_SPI_SDCARD_H

#pragma once

#include <stdint.h>

typedef enum { SD_TYPE_V2 = 0, SD_TYPE_HC } sd_type;

typedef enum {
	//REF Table 4-1:Overview of Card States vs. Operation Mode
	SD_STATE_IDLE = 0,
	SD_STATE_READY,
	SD_STATE_IDENT,
	SD_STATE_STBY,
	SD_STATE_TRAN,
	SD_STATE_DATA,
	SD_STATE_DATA_MULTI, // synthetical state for this implementation
	SD_STATE_RCV,
	SD_STATE_PRG,
	SD_STATE_DIS,
	SD_STATE_INA,

	//FIXME Existing states which must be revisited
	SD_STATE_WRITE_WAITFE,
	SD_STATE_WRITE_DATA
} sd_state;

void card_initialise(const char *sd1, const char *sd2);
void send_data(int cardno, uint16_t count, sd_state new_state);
void do_command(int cardno);
void change_state(int cardno, sd_state new_state);

void latch_in(void);
void card_byte_in(int cardno, uint8_t m_latch_in);
uint8_t card_byte_out(int cardno);
void shift_out(void);

#endif /* MAME_MACHINE_SPI_SDCARD_H */
