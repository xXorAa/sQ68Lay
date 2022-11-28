/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <string>
#include <vector>

namespace options {

#ifdef QLAY_EMU
extern std::string sysrom;
extern uint32_t ramsize;
extern std::vector<std::string> mdvFiles;

#endif

int parse(int argc, char **argv);

} // namespace options
