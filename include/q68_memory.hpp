/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

constexpr std::size_t operator""_KiB(unsigned long long int x) {
  return 1024ULL * x;
}

constexpr std::size_t operator""_MiB(unsigned long long int x) {
  return 1024_KiB * x;
}

namespace emulator {

void q68AllocateMemory(void);
extern uint8_t *q68MemorySpace;
extern uint8_t *q68ScreenSpace;

constexpr uint32_t q68_ram_size             = 0x01C00000;

constexpr uint32_t q68_internal_io          = 0x18000;
constexpr uint32_t q68_internal_io_size     = 4_KiB;

constexpr uint32_t q68_external_io          = 0x1C000;
constexpr uint32_t q68_external_io_size     = 16_KiB;

constexpr uint32_t q68_screen               = 0xFE800000;
constexpr uint32_t q68_screen_size          = 4_MiB;

constexpr uint32_t q68_q40_io               = 0xFF000000;
constexpr uint32_t q68_q40_io_size          = 16_MiB;

} // namespace emulator
