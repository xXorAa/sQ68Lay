#pragma once

#ifndef QLAY_IPC_H
#define QLAY_IPC_H

#include <stdint.h>

void qlayInitIPC(void);
void wrZX8302(uint8_t data);
void wr8049(uint32_t addr, uint8_t data);
uint8_t readQLHw(uint32_t addr);
void wrmdvcntl(uint8_t data);
void writeMdvSer(uint8_t data);
void do_next_event(void);

#endif /* QLAY_IPC_H */
