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

}