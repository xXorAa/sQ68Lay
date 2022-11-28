/*
 * Copyright (c) 2022 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <string>

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"
#include "q68_memory.hpp"

namespace options {

#ifdef QLAY_EMU
std::string sysrom;
unsigned int ramsize;
std::vector<std::string> mdvFiles;
#endif

int parse(int argc, char **argv)
{
    CLI::App opt("Emulator Options Parser");

#ifdef QLAY_EMU
    opt.add_option("--mdv", mdvFiles, "mdv file (upto 8 times")->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);
    opt.add_option("--ramsize", ramsize, "amount of ram in K (max 640)")->default_val(128);
    opt.add_option("--sysrom", sysrom, "system rom")->default_str("JS.rom");
#endif

    CLI11_PARSE(opt, argc, argv);

#ifdef QLAY_EMU
    if (ramsize > 640) {
        std::cerr << "ramsize too large" << std::endl;
        return 0;
    }
    std::cout << "Ram Size: " << ramsize << std::endl;
    ramsize *= 1_KiB;
    ramsize += 128_KiB;
#endif

    return 1;
}

} // namespace options
