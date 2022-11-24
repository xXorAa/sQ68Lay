/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <iostream>
#include <queue>
#include <SDL.h>

#include "q68_hardware.hpp"

namespace emulator {

// ghost irq registers
uint8_t q68_pc_intr = 0;
uint8_t q68_kbd_status = kbd_isint; // interrupt driven kbd

static int key_hack_idx = 0;

// keyboard lock and queue
SDL_SpinLock q68_kbd_lock = 0;
std::queue<uint8_t> q68_kbd_queue;

static uint8_t keys[3] = {0x1c, 0xF0, 0x1C};

unsigned int q68_read_hw_8(unsigned int addr)
{
    int kcode = 0;

    std::cout << "HWR8: " << std::hex << addr << std::endl;
    switch (addr) {
        case pc_intr:
            return q68_pc_intr;
        case kbd_code:
            SDL_AtomicLock(&q68_kbd_lock);
            if (q68_kbd_queue.size()) {
                kcode = q68_kbd_queue.front();
            }
            SDL_AtomicUnlock(&q68_kbd_lock);
            return kcode;
        case kbd_status:
            return q68_kbd_status;
        default:
            break;
    }

    return 0;
}

unsigned int q68_read_hw_16(unsigned int addr)
{
    std::cout << "HWR16: " << std::hex << addr << std::endl;

    return 0;
}

unsigned int q68_read_hw_32(unsigned int addr)
{
    std::cout << "HWR32: " << std::hex << addr << std::endl;

    return 0;
}

void q68_write_hw_8(unsigned int addr, unsigned int val)
{
    std::cout << "HWW8: " << std::hex << addr << "," << val << std::endl;
    switch (addr) {
        case pc_intr:
            q68_pc_intr &= ~val;
            return;
        case kbd_code:
            return;
        case kbd_unlock:
            SDL_AtomicLock(&q68_kbd_lock);
            if (val & kbd_ackn) {
                // code is acknowledged so remove it
                if(q68_kbd_queue.size()) {
                    q68_kbd_queue.pop();
                }
                // if the queue is empty clear the interrupt
                if (!q68_kbd_queue.size()) {
                    q68_kbd_status &= ~kbd_ackn;
                }
            }
            SDL_AtomicUnlock(&q68_kbd_lock);
            return;
        default:
            break;
    }
}

void q68_write_hw_16(unsigned int addr, unsigned int val)
{
    std::cout << "HWW16: " << std::hex << addr << "," << val << std::endl;
}

void q68_write_hw_32(unsigned int addr, unsigned int val)
{
    std::cout << "HWW32: " << std::hex << addr << "," << val << std::endl;
}

} // namespace emulator
