/*
        QLAY - Sinclair QL emulator
        Copyright Jan Venema 1998
        qldisk defines and functions
*/
#pragma once

#ifndef QLAY_DISK_H
#define QLAY_DISK_H

#include <stdint.h>

extern void qlayInitDisk(void);
extern void exit_qldisk(void);
extern void wrnfa(uint32_t, uint8_t);
extern uint8_t rdnfa(uint32_t);
extern int win_avail(void);

extern uint8_t rdserpar(uint32_t);
extern void wrserpar(uint32_t, uint8_t);

// Led settings. - By Jimmy
#define Power_Led_X 10
#define Power_Led_Y 350
#define First_Led_X 480
#define First_Led_Y 350
#define LED_YELLOW 6
#define LED_RED 2
#define LED_BLACK 0
#define LED_GREEN 4

#endif /* QLAY_DISK_H */
