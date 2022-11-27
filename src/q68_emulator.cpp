/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <SDL.h>
#include <string>

#include "m68k.h"
#include "q68_events.hpp"
#include "q68_hardware.hpp"
#include "q68_memory.hpp"
#include "q68_screen.hpp"

namespace emulator {

uint8_t *q68MemorySpace;
uint8_t *q68ScreenSpace;

void q68AllocateMemory()
{
    q68MemorySpace = new uint8_t[q68_ram_size];
    q68ScreenSpace = new uint8_t[q68_screen_size];
}

void q68LoadFile(std::string name, uint8_t *addr)
{
    //if (name[0] == '~') {
    //    name.erase(0, 1);
    //    name.insert(0, "/");
    //    name.insert(0, homedir);
    //}

    std::filesystem::path p{name};
    auto size = std::filesystem::file_size(p);

    std::ifstream romFile(name, std::ios::binary);
    if (romFile.bad()) {
        throw std::runtime_error("File NOT Found");
    }
    romFile.read((char *)addr, size);
    romFile.close();
}

int q68MainLoop(void *ptr)
{
    q68LoadFile("Q68_SMSQ.bin", q68MemorySpace + 0x320000);

    m68k_set_cpu_type(M68K_CPU_TYPE_68000_TG68K);
    m68k_init();
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_PC, 0x320000);

    uint64_t ticks = SDL_GetPerformanceFrequency() / 50;
    uint64_t then = SDL_GetPerformanceCounter();

    while(!exitLoop) {
        bool irq = false;

        m68k_execute(100000);

        uint64_t now = SDL_GetPerformanceCounter();

        if ((now - then) > ticks) {
            then = (then + ticks);
            q68RenderScreenFlag = true;
            q68_pc_intr |= pc_intrf;
            irq = true;
        }

        SDL_AtomicLock(&q68_kbd_lock);
        if (q68_kbd_queue.size()) {
            q68_kbd_status |= kbd_rcv;
            irq = true;
        }
        SDL_AtomicUnlock(&q68_kbd_lock);

        if (irq) {
            m68k_set_irq(2);
        }
    }

    return 0;
}

}

extern "C" {

    unsigned int m68k_read_memory_8(unsigned int address)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            return emulator::q68_read_hw_8(address);
        }

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

        return emulator::q68MemorySpace[address];
    }

    unsigned int m68k_read_memory_16(unsigned int address)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            return emulator::q68_read_hw_16(address);
        }

        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {
            return emulator::q68_read_hw_16(address);
        }

        if (address >= emulator::q68_q40_io) {
            return emulator::q68_read_hw_16(address);
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            return SDL_SwapBE16(*(uint16_t *)&emulator::q68ScreenSpace[address - emulator::q68_screen]);
        }

        if (address >= emulator::q68_ram_size) {
            return 0;
        }

        return SDL_SwapBE16(*(uint16_t *)&emulator::q68MemorySpace[address]);
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
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            return emulator::q68_read_hw_32(address);
        }

        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {
            return emulator::q68_read_hw_32(address);
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            return SDL_SwapBE32(*(uint32_t *)&emulator::q68ScreenSpace[address - emulator::q68_screen]);
        }

        if (address >= emulator::q68_q40_io) {
            return emulator::q68_read_hw_16(address);
        }

        if (address >= emulator::q68_ram_size) {
            return 0;
        }

        return SDL_SwapBE32(*(uint32_t *)&emulator::q68MemorySpace[address]);
    }

    unsigned int m68k_read_disassembler_32(unsigned int address)
    {
        if (address >= 32_MiB) {
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

        emulator::q68MemorySpace[address] = value;
    }

    void m68k_write_memory_16(unsigned int address, unsigned int value)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            emulator::q68_write_hw_16(address, value);
            return;
        }

        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {
            emulator::q68_write_hw_16(address, value);
            return;
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            *(uint16_t *)&emulator::q68ScreenSpace[address - emulator::q68_screen] = SDL_SwapBE16(value);
            return;
        }

        if (address >= emulator::q68_q40_io) {
            emulator::q68_write_hw_16(address, value);
            return;
        }

        if (address >= emulator::q68_ram_size) {
            return;
        }

        *(uint16_t *)&emulator::q68MemorySpace[address] = SDL_SwapBE16(value);
    }

    void m68k_write_memory_32(unsigned int address, unsigned int value)
    {
        if ((address >= emulator::q68_internal_io) &&
            address < (emulator::q68_internal_io + emulator::q68_internal_io_size)) {
            emulator::q68_write_hw_32(address, value);
            return;
        }

        if ((address >= emulator::q68_external_io) &&
            address < (emulator::q68_external_io + emulator::q68_external_io_size)) {
            emulator::q68_write_hw_32(address, value);
            return;
        }

        if ((address >= emulator::q68_screen) &&
            address < (emulator::q68_screen + emulator::q68_screen_size)) {

            *(uint32_t *)&emulator::q68ScreenSpace[address - emulator::q68_screen] = SDL_SwapBE32(value);
            return;
        }

        if (address >= emulator::q68_q40_io) {
            emulator::q68_write_hw_32(address, value);
            return;
        }

        if (address >= emulator::q68_ram_size) {
            return;
        }

        *(uint32_t *)&emulator::q68MemorySpace[address] = SDL_SwapBE32(value);
    }
}
