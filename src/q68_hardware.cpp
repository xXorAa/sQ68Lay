/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <chrono>
#include <ctime>
#include <iostream>
#include <queue>
#include <SDL.h>

#include "q68_hardware.hpp"
#include "q68_screen.hpp"

namespace emulator {

// ghost irq registers
uint8_t q68_pc_intr = 0;
uint8_t q68_mc_stat = 0;
uint8_t q68_kbd_status = kbd_isint; // interrupt driven kbd
uint8_t q68_q68_dmode = 0;

static int key_hack_idx = 0;

// keyboard lock and queue
SDL_SpinLock q68_kbd_lock = 0;
std::queue<uint8_t> q68_kbd_queue;

static uint8_t keys[3] = {0x1c, 0xF0, 0x1C};

static uint32_t q68_update_time()
{
    const auto clk = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
                   clk.time_since_epoch()).count() + qdos_time;
}

#ifdef Q68_EMU
static uint32_t q68_update_hires()
{
    uint64_t freq = SDL_GetPerformanceFrequency();

    return SDL_GetPerformanceCounter() / (freq / 40000000);
}
#endif

unsigned int q68_read_hw_8(unsigned int addr)
{
    int kcode = 0;

    //std::cout << "HWR8: " << std::hex << addr << std::endl;
    switch (addr) {
        case pc_clock:
            return q68_update_time() >> 24;
        case pc_clock + 1:
            return (q68_update_time() >> 16) & 0xFF;
        case pc_clock + 2:
            return (q68_update_time() >> 8) & 0xFF;
        case pc_clock + 3:
            return q68_update_time() & 0xFF;
        case pc_intr:
            return q68_pc_intr;
#ifdef Q68_EMU
        case q68_timer:
            return q68_update_hires() >> 24;
        case q68_timer + 1:
            return (q68_update_hires() >> 16) & 0xFF;
        case q68_timer + 2:
            return (q68_update_hires() >> 8) & 0xFF;
        case q68_timer + 3:
            return q68_update_hires() & 0xFF;
        case kbd_code:
            SDL_AtomicLock(&q68_kbd_lock);
            if (q68_kbd_queue.size()) {
                kcode = q68_kbd_queue.front();
            }
            SDL_AtomicUnlock(&q68_kbd_lock);
            return kcode;
        case kbd_status:
            return q68_kbd_status;
        case q68_dmode:
            return q68_q68_dmode;
#endif
        default:
            break;
    }

    return 0;
}

unsigned int q68_read_hw_16(unsigned int addr)
{
    //std::cout << "HWR16: " << std::hex << addr << std::endl;

    switch(addr) {
        case pc_clock:
            return q68_update_time() >> 16;
        case pc_clock + 2:
            return q68_update_time() & 0xFFFF;
#ifdef Q68_EMU
        case q68_timer:
            return q68_update_hires() >> 16;
        case q68_timer + 2:
            return q68_update_hires() & 0xFFFF;
#endif
        default:
            break;
    }
    return 0;
}

unsigned int q68_read_hw_32(unsigned int addr)
{
    //std::cout << "HWR32: " << std::hex << addr << std::endl;
    switch (addr) {
        case pc_clock:
            return q68_update_time();
            break;
#ifdef Q68_EMU
        case q68_timer:
            return q68_update_hires();
#endif
        default:
            break;
    }
    return 0;
}

void q68_write_hw_8(unsigned int addr, unsigned int val)
{
    //std::cout << "HWW8: " << std::hex << addr << "," << val << std::endl;
    switch (addr) {
        case pc_intr:
            q68_pc_intr &= ~val;
            return;
        case mc_stat:
            q68_mc_stat = val;
            q68ScreenChangeMode(val >> 3);
            return;
#ifdef Q68_EMU
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
        case q68_dmode:
            q68_q68_dmode = val;
            q68ScreenChangeMode(val & 7);
            return;
#endif
        default:
            break;
    }
}

void q68_write_hw_16(unsigned int addr, unsigned int val)
{
    //std::cout << "HWW16: " << std::hex << addr << "," << val << std::endl;
}

void q68_write_hw_32(unsigned int addr, unsigned int val)
{
    //std::cout << "HWW32: " << std::hex << addr << "," << val << std::endl;
}

} // namespace emulator
