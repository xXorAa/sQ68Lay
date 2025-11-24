/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "args.h"
#pragma GCC diagnostic pop

#include "ini.h"
#include "utlist.h"
#include "utstring.h"
#include "version.h"

#ifdef Q68_EMU
static const char* const helpTextInit = "\n\
Usage: sq68ux [OPTIONS] [args...]\n\
\n\
Positionals:\n\
  args                        Arguments passed to QDOS\n\
\n\
Options:\n\
  -h,--help                   Print this help message and exit\n\
  -f,--CONFIG [sq68ux.ini]    Read an ini file\n";
#endif

#ifdef QLAY_EMU
static const char* const helpTextInit = "\n\
Usage: sqlay3 [OPTIONS] [qlpak file]\n\
\n\
Positionals:\n\
  qlpak file                  Q-Emulator style qlpak file\n\
\n\
Options:\n\
  -h,--help                   Print this help message and exit\n\
  -f,--CONFIG [sqlay3.ini]    Read an ini file\n";
#endif

static const char* const helpTextTail = "\
  --version                   version number\n";

enum emuOptsType {
  EMU_OPT_INT,
  EMU_OPT_CHAR,
  EMU_OPT_DEV,
};

struct emuList {
  char* option;
  struct emuList* next;
};

struct emuOpts {
  char* option;
  char* alias;
  char* help;
  int type;
  int intVal;
  char* charVal;
  struct emuList* list;
};

struct emuOpts emuOptions[] = {
#ifdef SQLUX_EMU

  { "bdi1", "", "file exposed by the BDI interface", EMU_OPT_CHAR, 0,
      NULL },
  { "boot_cmd", "b", "command to run on boot (executed in basic)",
      EMU_OPT_CHAR, 0, NULL },
  { "boot_device", "d", "device to load BOOT file from", EMU_OPT_CHAR, 0,
      "mdv1" },
  { "cpu_hog", "", "1 = use all cpu, 0 = sleep when idle", EMU_OPT_INT, 1,
      NULL },
  { "device", "", "QDOS_name,path,flags (may be used multiple times",
      EMU_OPT_DEV, 0, NULL },
  { "fast_startup", "", "1 = skip ram test (does not affect Minerva)",
      EMU_OPT_INT, 0, NULL },
  { "filter", "", "enable bilinear filter when zooming", EMU_OPT_INT, 0,
      NULL },
  { "fixaspect", "",
      "0 = 1:1 pixel mapping, 1 = 2:3 non square pixels, 2 = BBQL aspect non square pixels",
      EMU_OPT_INT, 0, NULL },
  { "iorom1", "", "rom in 1st IO area (Minerva only 0x10000 address)",
      EMU_OPT_CHAR, 0, NULL },
  { "iorom2", "", "rom in 2nd IO area (Minerva only 0x14000 address)",
      EMU_OPT_CHAR, 0, NULL },
  { "joy1", "", "1-8 SDL2 joystick index", EMU_OPT_INT, 0, NULL },
  { "joy2", "", "1-8 SDL2 joystick index", EMU_OPT_INT, 0, NULL },
  { "kbd", "", "keyboard language DE, GB, US", EMU_OPT_CHAR, 0, "US" },
  { "no_patch", "n", "disable patching the rom", EMU_OPT_INT, 0, NULL },
  { "palette", "",
      "0 = Full colour, 1 = Unsaturated colours (slightly more CRT like), 2 =  Enable grayscale display",
      EMU_OPT_INT, 0, NULL },
  { "print", "", "command to use for print jobs", EMU_OPT_CHAR, 0,
      "lpr" },
  { "ramtop", "r",
      "The memory space top (128K + QL ram, not valid if ramsize set)",
      EMU_OPT_INT, 256, NULL },
  { "ramsize", "", "The size of ram", EMU_OPT_INT, 0, NULL },
  { "resolution", "g", "resolution of screen in mode 4", EMU_OPT_CHAR, 0,
      "512x256" },
  { "romdir", "", "path to the roms", EMU_OPT_CHAR, 0, "roms" },
  { "romport", "", "rom in QL rom port (0xC000 address)", EMU_OPT_CHAR, 0,
      NULL },
  { "romim", "",
      "rom in QL rom port (0xC000 address, legacy alias for romport)",
      EMU_OPT_CHAR, 0, NULL },
  { "ser1", "", "device for ser1", EMU_OPT_CHAR, 0, NULL },
  { "ser2", "", "device for ser2", EMU_OPT_CHAR, 0, NULL },
  { "ser3", "", "device for ser3", EMU_OPT_CHAR, 0, NULL },
  { "ser4", "", "device for ser4", EMU_OPT_CHAR, 0, NULL },
  { "shader", "",
      "0 = Disabled, 1 = Use flat shader, 2 = Use curved shader",
      EMU_OPT_INT, 0, NULL },
  { "shader_file", "", "Path to shader file to use if SHADER is 1 or 2",
      EMU_OPT_CHAR, 0, "shader.glsl" },
  { "skip_boot", "", "1 = skip f1/f2 screen, 0 = show f1/f2 screen",
      EMU_OPT_INT, 1, NULL },
  { "sound", "", "volume in range 1-8, 0 to disable", EMU_OPT_INT, 0,
      NULL },
  { "speed", "", "speed in factor of BBQL speed, 0.0 for full speed",
      EMU_OPT_CHAR, 0, "0.0" },
  { "strict_lock", "", "enable strict file locking", EMU_OPT_INT, 0,
      NULL },
  { "sysrom", "", "system rom", EMU_OPT_CHAR, 0, "MIN198.rom" },
  { "win_size", "w", "window size 1x, 2x, 3x, max, full", EMU_OPT_CHAR, 0,
      "1x" },
  { "verbose", "v", "verbosity level 0-3", EMU_OPT_INT, 1, NULL },
#endif // SQLUX_EMU

#ifdef QLAY_EMU
  { "ayvol", "", "volume of QSound sound in range 0-10", EMU_OPT_INT, 3,
      NULL, NULL },
  { "drive", "l", "winX@dir or mdvX@file (upto 16 times)", EMU_OPT_DEV, 0,
      NULL, NULL },
  { "exprom", "c", "romfile@address eg NFA.rom@0xC000", EMU_OPT_DEV, 0,
      NULL, NULL },
  { "fastfps", "", "The FPS when running in fast mode, standard is 50", EMU_OPT_INT, 200,
      NULL, NULL },
  { "ipcvol", "", "volume of IPC sound in range 0-10", EMU_OPT_INT, 3,
      NULL, NULL },
  { "mdvvol", "", "volume of MDV sound effect in range 0-10", EMU_OPT_INT,
      3, NULL, NULL },
  { "qlsd", "", "turn on qlsd emulation", EMU_OPT_INT, 0, NULL, NULL },
  { "qsound", "", "address in hex of qsound registers", EMU_OPT_CHAR, 0,
      NULL, NULL },
  { "ramsize", "m", "amount of ram in K (max 8192)", EMU_OPT_INT, 128,
      NULL, NULL },
  { "sysrom", "r", "system rom", EMU_OPT_CHAR, 0, "JS.rom", NULL },
  { "turboload", "", "run emulator at max speed while MDV motor on",
      EMU_OPT_INT, 0, NULL, NULL },
#endif // QLAY_EMU

#ifdef Q68_EMU
  { "smsqe", "", "smsqe image to load (at 0x32000)", EMU_OPT_CHAR, 0,
      NULL, NULL },
  { "sssvol", "", "volume of SSS sound in range 0-10", EMU_OPT_INT, 3,
      NULL, NULL },
  { "sysrom", "r", "system rom to load (at 0x0)", EMU_OPT_CHAR, 0, NULL,
      NULL },
#endif
  { "sd1", "", "SDHC Image for SD1 slot", EMU_OPT_CHAR, 0, NULL, NULL },
  { "sd2", "", "SDHC Image for SD1 slot", EMU_OPT_CHAR, 0, NULL, NULL },
  { "trace", "", "enable tracing", EMU_OPT_INT, 0, NULL, NULL },
  { "trace-high", "", "highest address to trace", EMU_OPT_INT, 0xFFFFFF,
      NULL, NULL },
  { "trace-low", "", "lowest address to trace", EMU_OPT_INT, 0, NULL,
      NULL },
  { "trace-map", "", "map file for trace addrs to symbols", EMU_OPT_CHAR,
      0, NULL, NULL },

  { NULL, NULL, NULL, 0, 0, NULL, NULL },
};

static ArgParser* parser;

static bool match(const char* name, const char* option)
{
  return (strcasecmp(name, option) == 0);
}

static int iniHandler(__attribute__((unused)) void* user,
    __attribute__((unused)) const char* section,
    const char* name, const char* value)
{
  int i;

  i = 0;
  while (emuOptions[i].option != NULL) {
    if (match(name, emuOptions[i].option)) {
      if (emuOptions[i].type == EMU_OPT_CHAR) {
        emuOptions[i].charVal = strdup(value);
        return 0;
      } else if (emuOptions[i].type == EMU_OPT_INT) {
        emuOptions[i].intVal = atoi(value);
        return 0;
      } else if (emuOptions[i].type == EMU_OPT_DEV) {
        struct emuList* entry;

        entry = malloc(sizeof(struct emuList));
        if (entry) {
          entry->option = strdup(value);
          LL_APPEND(emuOptions[i].list, entry);
        }
      }

      return 1;
    }

    i++;
  }

  return 1;
}

int emulatorOptionParse(int argc, char** argv)
{
  int i;
  UT_string* helptext;
  const char* configFile;

  utstring_new(helptext);
  utstring_printf(helptext, "%s", helpTextInit);

  parser = ap_new_parser();
  if (!parser) {
    exit(1);
  }

  i = 0;
  while (emuOptions[i].option != NULL) {
    int j;
    UT_string* helpItem;
    utstring_new(helpItem);

    if (strlen(emuOptions[i].alias)) {
      utstring_printf(helpItem, "  -%s,",
          emuOptions[i].alias);
    } else {
      utstring_printf(helpItem, "  ");
    }
    utstring_printf(helpItem, "--%s", emuOptions[i].option);

    if (emuOptions[i].type == EMU_OPT_INT) {
      utstring_printf(helpItem, " [%d]",
          emuOptions[i].intVal);
    } else if (emuOptions[i].type == EMU_OPT_CHAR) {
      if (emuOptions[i].charVal != NULL) {
        utstring_printf(helpItem, " [%s]",
            emuOptions[i].charVal);
      }
    }
    for (j = utstring_len(helpItem); j < 30; j++) {
      utstring_printf(helpItem, " ");
    }

    utstring_printf(helpItem, "%s\n", emuOptions[i].help);

    utstring_concat(helptext, helpItem);

    utstring_free(helpItem);

    i++;
  }
  utstring_printf(helptext, "%s", helpTextTail);

  ap_set_helptext(parser, utstring_body(helptext));
  ap_set_version(parser, release);

  utstring_free(helptext);

  ap_add_str_opt(parser, "config f", EMU_STR ".ini");

  i = 0;
  while (emuOptions[i].option != NULL) {
    UT_string* optItem;
    utstring_new(optItem);

    if (strlen(emuOptions[i].alias)) {
      utstring_printf(optItem, "%s %s", emuOptions[i].option,
          emuOptions[i].alias);
    } else {
      utstring_printf(optItem, "%s", emuOptions[i].option);
    }

    if (emuOptions[i].type == EMU_OPT_INT) {
      ap_add_int_opt(parser, utstring_body(optItem), 0);
    } else {
      ap_add_str_opt(parser, utstring_body(optItem), "");
    }

    utstring_free(optItem);

    i++;
  }

  if (!ap_parse(parser, argc, argv)) {
    exit(1);
  }

  configFile = ap_get_str_value(parser, "config");

  if (ini_parse(configFile, iniHandler, NULL) < 0) {
    fprintf(stderr, "Can't load '%s'\n", configFile);
    return 1;
  }

  return 0;
}

void emulatorOptionsRemove(void)
{
  ap_free(parser);
}

const char* emulatorOptionString(const char* name)
{
  int i;

  if (ap_count(parser, name)) {
    return ap_get_str_value(parser, name);
  }

  i = 0;
  while (emuOptions[i].option != NULL) {
    if ((strcmp(emuOptions[i].option, name) == 0) && (emuOptions[i].type == EMU_OPT_CHAR) && (emuOptions[i].charVal != NULL)) {
      return emuOptions[i].charVal;
    }
    i++;
  }

  return "";
}

int emulatorOptionInt(const char* name)
{
  int i;

  if (ap_count(parser, name)) {
    return ap_get_int_value(parser, name);
  }

  i = 0;
  while (emuOptions[i].option != NULL) {
    if (strcmp(emuOptions[i].option, name) == 0) {
      return emuOptions[i].intVal;
    }
    i++;
  }

  return 0;
}

int emulatorOptionDevCount(const char* name)
{
  int i, count;

  count = ap_count(parser, name);

  i = 0;
  while (emuOptions[i].option != NULL) {
    if (strcmp(emuOptions[i].option, name) == 0) {
      if (emuOptions[i].list != NULL) {
        int tmpCount;
        struct emuList* entry;
        LL_COUNT(emuOptions[i].list, entry, tmpCount);
        count += tmpCount;
      }
    }

    i++;
  }

  return count;
}

const char* emulatorOptionDev(const char* name, int idx)
{
  int i = 0;

  if (idx < ap_count(parser, name)) {
    return ap_get_str_value_at_index(parser, name, idx);
  }

  idx -= ap_count(parser, name);

  while (emuOptions[i].option != NULL) {
    if (strcmp(emuOptions[i].option, name) == 0) {
      if (emuOptions[i].list != NULL) {
        int cur = 0;
        struct emuList* entry;
        LL_FOREACH(emuOptions[i].list, entry)
        {
          if (idx == cur) {
            return entry->option;
          }
          cur++;
        }
      }
    }
    i++;
  }

  return NULL;
}

int emulatorOptionArgc(void)
{
  return ap_count_args(parser);
}

char* emulatorOptionArgv(int idx)
{
  return ap_get_arg_at_index(parser, idx);
}
