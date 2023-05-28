#pragma once

#ifndef Q68_SD_H
#define Q68_SD_H

void q68InitSD(void);
void q68ProcessSDCmd(int sd, uint8_t cmdByte);
uint8_t q68ProcessSDResponse(int sd);

#endif /* Q68_SD_H */
