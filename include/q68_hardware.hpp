/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <cstdint>

namespace emulator {

extern uint8_t q68_pc_intr;

constexpr uint32_t pc_intr = 0x18021;
constexpr uint8_t pc_intrf = 0x08;

unsigned int q68_read_hw_8(unsigned int addr);
void q68_write_hw_8(unsigned int addr, unsigned int val);

} // namespace emulator