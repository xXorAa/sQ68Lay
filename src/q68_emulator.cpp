/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <SDL.h>
#include <string>

#include "m68k.h"
#include "emulator_options.h"
#include "q68_events.hpp"
#include "q68_hardware.hpp"
#include "q68_memory.hpp"
#include "q68_screen.hpp"
#include "qldisk.hpp"
#include "qlio.hpp"

namespace emulator {

uint8_t *q68MemorySpace;
uint8_t *q68ScreenSpace;
uint32_t msClk = 0;
uint32_t msClkNextEvent = 0;

uint64_t cycleTick;

uint32_t cyclesThen = 0;

uint32_t msCyclesNow;

uint32_t cycleNextEvent;

bool doIrq;

void q68AllocateMemory()
{
    q68MemorySpace = new uint8_t[q68_ram_size];
    q68ScreenSpace = new uint8_t[q68_screen_size];
}

int q68MainLoop(void *ptr)
{
    char *exprom;
    int  expromCount;

    try {
#ifdef Q68_EMU
    q68LoadFile("Q68_SMSQ.bin", q68MemorySpace + 0x320000);
#endif
#ifdef QLAY_EMU
    q68LoadFile(emulatorOptionString("sysrom"), q68MemorySpace);

    expromCount = emulatorOptionDevCount("exprom");
    if (expromCount) {
        q68LoadFile(emulatorOptionDev("exprom", 0), q68MemorySpace + 48_KiB);
    }
#endif
    } catch (std::exception &e) {
        std::cerr << "Failed to load " << e.what() << std::endl;
        return 0;
    }

#ifdef Q68_EMU
    m68k_set_cpu_type(M68K_CPU_TYPE_68000_TG68K);
#endif
#ifdef QLAY_EMU
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
#endif
    m68k_init();
    m68k_pulse_reset();
#ifdef Q68_EMU
    m68k_set_reg(M68K_REG_PC, 0x320000);
#endif
#ifdef QLAY_EMU
    ipc::initIPC();
    qldisk::init_qldisk();
#endif

    uint64_t counterFreq = SDL_GetPerformanceFrequency();
    uint64_t screenTick = counterFreq / 50;
    uint64_t msTick = counterFreq / 10000;

    uint64_t screenThen = SDL_GetPerformanceCounter();
    uint64_t msThen = screenThen;

#ifdef QLAY_EMU
    int ql_loops = 0;
#endif

    while(!exitLoop) {
        bool irq = false;

#ifdef Q68_EMU
        m68k_execute(50000);
#endif
        uint64_t now = SDL_GetPerformanceCounter();

#ifdef Q68_EMU
        if ((now - screenThen) > screenTick) {
            screenThen = (screenThen + screenTick);
            q68RenderScreenFlag = true;
            q68_pc_intr |= pc_intrf;
            irq = true;
        }
#endif

#ifdef QLAY_EMU
        if ((now - msThen) > msTick) {
            msThen = (msThen + msTick);
            msClk++;
            m68k_execute(750);
            cyclesThen += 750;
        }
#endif

#ifdef Q68_EMU
        SDL_AtomicLock(&q68_kbd_lock);
        if (q68_kbd_queue.size()) {
            q68_kbd_status |= kbd_rcv;
            irq = true;
        }
        SDL_AtomicUnlock(&q68_kbd_lock);

        if (irq) {
            m68k_set_irq(2);
            irq = false;
        }
#endif
    }

    return 0;
}

uint32_t cycles()
{
    return cyclesThen + m68k_cycles_run();
}

}

extern "C" {

    unsigned int m68k_read_memory_8(unsigned int address)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            return emulator::q68_read_hw_8(address);
        }
#ifdef QLAY_EMU
        if ((address >= emulator::qlay_nfa_io) &&
            address < (emulator::qlay_nfa_io + emulator::qlay_nfa_io_size)) {
            return qldisk::rdnfa(address);
        }
#endif
#ifdef Q68_EMU
        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {

            // temp disable MMC
            if ((address < 0x1c300) || (address >0x1c358)) {
                return emulator::q68_read_hw_8(address);
            }
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            return emulator::q68ScreenSpace[address - emulator::q68_screen];
        }

        if (address >= emulator::q68_q40_io) {
            return emulator::q68_read_hw_8(address);
        }

        if (address >= emulator::q68_ram_size) {
            return 0;
        }
#endif
#ifdef QLAY_EMU
        if (address >= 256_KiB) {
            return 0;
        }
        if ((address >= 128_KiB) && (address < 256_KiB)) {
            //m68k_modify_timeslice(-2);
        }
#endif
        return emulator::q68MemorySpace[address];
    }

    unsigned int m68k_read_memory_16(unsigned int address)
    {
        m68k_modify_timeslice(-4);
        return m68k_read_memory_8(address) << 8 |
            m68k_read_memory_8(address + 1);
    }

    unsigned int  m68k_read_disassembler_16(unsigned int address)
    {
        if (address >= emulator::q68_ram_size) {
            return 0;
        }

        return SDL_SwapBE16(*(uint16_t *)&emulator::q68MemorySpace[address]);
    }

    unsigned int m68k_read_memory_32(unsigned int address)
    {
        m68k_modify_timeslice(-12);
        return m68k_read_memory_8(address) << 24 |
            m68k_read_memory_8(address + 1) << 16 |
            m68k_read_memory_8(address + 2) << 8 |
            m68k_read_memory_8(address + 3);
    }

    unsigned int m68k_read_disassembler_32(unsigned int address)
    {
        if (address >= emulator::q68_ram_size) {
            return 0;
        }

        return SDL_SwapBE32(*(uint32_t *)&emulator::q68MemorySpace[address]);
    }

    void m68k_write_memory_8(unsigned int address, unsigned int value)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            emulator::q68_write_hw_8(address, value);
            return;
        }
#ifdef QLAY_EMU
        if ((address >= emulator::qlay_nfa_io) &&
            address < (emulator::qlay_nfa_io + emulator::qlay_nfa_io_size)) {
            qldisk::wrnfa(address, value);
            emulator::q68MemorySpace[address] = value;
        }
#endif
#ifdef Q68_EMU
        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {

            // temp disable MMC
            if ((address < 0x1c300) || (address >0x1c358)) {
                emulator::q68_write_hw_8(address, value);
            }
            return;
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            emulator::q68ScreenSpace[address - emulator::q68_screen] = value;
            return;
        }

        if (address >= emulator::q68_q40_io) {
            emulator::q68_write_hw_8(address, value);
            return;
        }

        if (address >= emulator::q68_ram_size) {
            return;
        }
#endif
#ifdef QLAY_EMU
        if ((address >= 128_KiB) && (address < 256_KiB)) {
            m68k_modify_timeslice(-6);
        }
#endif

        emulator::q68MemorySpace[address] = value;
    }

    void m68k_write_memory_16(unsigned int address, unsigned int value)
    {
        m68k_modify_timeslice(-4);
        m68k_write_memory_8(address + 0, (value >> 8) & 0xFF);
        m68k_write_memory_8(address + 1, (value >> 0) & 0xFF);
    }

    void m68k_write_memory_32(unsigned int address, unsigned int value)
    {
        m68k_modify_timeslice(-12);
        m68k_write_memory_8(address + 0, (value >> 24) & 0xFF);
        m68k_write_memory_8(address + 1, (value >> 16) & 0xFF);
        m68k_write_memory_8(address + 2, (value >> 8) & 0xFF);
        m68k_write_memory_8(address + 3, (value >> 0) & 0xFF);
    }

    void emu_hook_pc(unsigned int pc)
    {
#ifdef QLAY_EMU
        if (emulator::cycles() >= emulator::cycleNextEvent) {
            ipc::do_next_event();
        }

        if (emulator::doIrq) {
            if (emulator::q68_pc_intr & emulator::pc_intrf) {
                emulator::q68RenderScreenFlag = true;
            }
            m68k_set_irq(2);
            emulator::doIrq = 0;
        }

        //char disBuf[256];
        //m68k_disassemble(disBuf, pc, M68K_CPU_TYPE_68000);
        //std::cout << std::hex << std::setfill('0') << std::setw(4) << pc << " " << disBuf << std::endl;
#endif
    }
}
