/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>

#include "q68_hardware.hpp"

namespace emulator {

uint8_t q68_pc_intr;

unsigned int q68_read_hw_8(unsigned int addr)
{
    std::cout << "HWR8: " << std::hex << addr << std::endl;
    switch (addr) {
        case pc_intr:
            return q68_pc_intr;
        case 0x1c140:
            return 0x1c;
        default:
            break;
    }

    return 0;
}

void q68_write_hw_8(unsigned int addr, unsigned int val)
{
    std::cout << "HWW8: " << std::hex << addr << "," << val << std::endl;
    switch (addr) {
        case pc_intr:
            q68_pc_intr &= ~val;
            return;
        case 0x1c140:
            return;
        default:
            break;
    }
}

} // namespace emulator
