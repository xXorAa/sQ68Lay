/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <ctype.h>
#include <glib.h>
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
#include "version.h"


static const char * const helpTextInit = "\n\
Usage: sqlux [OPTIONS] [args...]\n\
\n\
Positionals:\n\
  args                        Arguments passed to QDOS\n\
\n\
Options:\n\
  -h,--help                   Print this help message and exit\n\
  -f,--CONFIG [sqlux.ini]     Read an ini file\n";

static const char * const helpTextTail = "\
  --version                   version number\n";


enum emuOptsType {
	EMU_OPT_INT,
	EMU_OPT_CHAR,
	EMU_OPT_DEV,
};

struct emuOpts {
	char *option;
	char *alias;
	char *help;
	int type;
	int intVal;
	char *charVal;
	GList *list;
};

struct emuOpts emuOptions[] = {
#ifdef SQLUX_EMU

{"bdi1", "", "file exposed by the BDI interface", EMU_OPT_CHAR, 0 , NULL},
{"boot_cmd", "b", "command to run on boot (executed in basic)", EMU_OPT_CHAR, 0, NULL},
{"boot_device", "d", "device to load BOOT file from", EMU_OPT_CHAR, 0, "mdv1"},
{"cpu_hog", "", "1 = use all cpu, 0 = sleep when idle", EMU_OPT_INT, 1, NULL},
{"device", "", "QDOS_name,path,flags (may be used multiple times", EMU_OPT_DEV, 0, NULL},
{"fast_startup", "", "1 = skip ram test (does not affect Minerva)", EMU_OPT_INT, 0, NULL},
{"filter", "", "enable bilinear filter when zooming", EMU_OPT_INT, 0, NULL},
{"fixaspect", "", "0 = 1:1 pixel mapping, 1 = 2:3 non square pixels, 2 = BBQL aspect non square pixels", EMU_OPT_INT, 0, NULL},
{"iorom1", "", "rom in 1st IO area (Minerva only 0x10000 address)", EMU_OPT_CHAR, 0, NULL},
{"iorom2", "", "rom in 2nd IO area (Minerva only 0x14000 address)", EMU_OPT_CHAR, 0, NULL},
{"joy1", "", "1-8 SDL2 joystick index", EMU_OPT_INT, 0, NULL},
{"joy2", "", "1-8 SDL2 joystick index", EMU_OPT_INT, 0, NULL},
{"kbd", "", "keyboard language DE, GB, US", EMU_OPT_CHAR, 0, "US"},
{"no_patch", "n", "disable patching the rom", EMU_OPT_INT, 0, NULL},
{"palette", "", "0 = Full colour, 1 = Unsaturated colours (slightly more CRT like), 2 =  Enable grayscale display", EMU_OPT_INT, 0, NULL},
{"print", "", "command to use for print jobs", EMU_OPT_CHAR, 0, "lpr"},
{"ramtop", "r", "The memory space top (128K + QL ram, not valid if ramsize set)", EMU_OPT_INT, 256, NULL},
{"ramsize", "", "The size of ram", EMU_OPT_INT, 0, NULL},
{"resolution", "g", "resolution of screen in mode 4", EMU_OPT_CHAR, 0, "512x256"},
{"romdir", "", "path to the roms", EMU_OPT_CHAR, 0, "roms"},
{"romport", "", "rom in QL rom port (0xC000 address)", EMU_OPT_CHAR, 0, NULL},
{"romim", "", "rom in QL rom port (0xC000 address, legacy alias for romport)", EMU_OPT_CHAR, 0, NULL},
{"ser1", "", "device for ser1", EMU_OPT_CHAR, 0, NULL},
{"ser2", "", "device for ser2", EMU_OPT_CHAR, 0, NULL},
{"ser3", "", "device for ser3", EMU_OPT_CHAR, 0, NULL},
{"ser4", "", "device for ser4", EMU_OPT_CHAR, 0, NULL},
{"shader", "", "0 = Disabled, 1 = Use flat shader, 2 = Use curved shader", EMU_OPT_INT, 0, NULL},
{"shader_file", "", "Path to shader file to use if SHADER is 1 or 2", EMU_OPT_CHAR, 0, "shader.glsl"},
{"skip_boot", "", "1 = skip f1/f2 screen, 0 = show f1/f2 screen", EMU_OPT_INT, 1, NULL},
{"sound", "", "volume in range 1-8, 0 to disable", EMU_OPT_INT, 0, NULL},
{"speed", "", "speed in factor of BBQL speed, 0.0 for full speed", EMU_OPT_CHAR, 0, "0.0"},
{"strict_lock", "", "enable strict file locking", EMU_OPT_INT, 0, NULL},
{"sysrom", "", "system rom", EMU_OPT_CHAR, 0, "MIN198.rom"},
{"win_size", "w", "window size 1x, 2x, 3x, max, full", EMU_OPT_CHAR, 0, "1x"},
{"verbose", "v", "verbosity level 0-3", EMU_OPT_INT, 1, NULL},
#endif //SQLUX_EMU

#ifdef QLAY_EMU
{"ramsize", "m", "amount of ram in K (max 8192)", EMU_OPT_INT, 128, NULL, NULL},
{"exprom", "c", "address@romfile eg C000@NFA.rom", EMU_OPT_DEV, 0, NULL, NULL},
{"sysrom", "r", "system rom", EMU_OPT_CHAR, 0, "JS.rom", NULL},
{"drive", "l", "nfa file (upto 8 times)", EMU_OPT_DEV, 0 ,NULL, NULL},
{"trace", "", "enable tracing", EMU_OPT_INT, 0, NULL, NULL},
#endif //QLAY_EMU

#ifdef Q68_EMU
{"smsqe", "", "smsqe image to load (at 0x32000)", EMU_OPT_CHAR, 0, NULL, NULL},
#endif

{NULL, NULL, NULL, 0, 0, NULL, NULL},
};


static ArgParser* parser;

static bool match(const char *name, const char *option)
{
	return (strcasecmp(name, option) == 0);
}

static int iniHandler(__attribute__ ((unused)) void* user, __attribute__ ((unused)) const char* section, const char* name,
                   const char* value)
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
				emuOptions[i].list = g_list_append(emuOptions[i].list, strdup(value));
			}

			return 1;
		}

		i++;
	}

	return 1;
}

int emulatorOptionParse(int argc, char **argv)
{
	int i;
	GString *helptext = g_string_new(helpTextInit);
	const char *configFile;

	parser = ap_new_parser();
	if (!parser) {
		exit(1);
	}

	i = 0;
	while (emuOptions[i].option != NULL) {
		int j;
		GString *helpItem = g_string_new("");

		if(strlen(emuOptions[i].alias)) {
			g_string_append_printf(helpItem, "  -%s,", emuOptions[i].alias);
		} else {
			 g_string_append(helpItem, "  ");
		}
		g_string_append_printf(helpItem, "--%s", emuOptions[i].option);

		if (emuOptions[i].type == EMU_OPT_INT) {
			g_string_append_printf(helpItem, " [%d]", emuOptions[i].intVal);
		} else if (emuOptions[i].type == EMU_OPT_CHAR) {
			if (emuOptions[i].charVal != NULL) {
				g_string_append_printf(helpItem, " [%s]", emuOptions[i].charVal);
			}
		}
		for (j = helpItem->len; j < 30; j++){
			g_string_append_c(helpItem, ' ');
		}

		g_string_append_printf(helpItem, "%s\n", emuOptions[i].help);

		g_string_append(helptext, helpItem->str);

		g_string_free(helpItem, true);

		i++;
	}
	g_string_append(helptext, helpTextTail);

	ap_set_helptext(parser, helptext->str);
	ap_set_version(parser, release);

	g_string_free(helptext, true);

	ap_add_str_opt(parser, "config f", EMU_STR ".ini");

	i = 0;
	while (emuOptions[i].option != NULL) {
		GString *optItem = g_string_new("");

		if (strlen(emuOptions[i].alias)) {
			g_string_printf(optItem, "%s %s",
				emuOptions[i].option,
				emuOptions[i].alias);
		} else {
			g_string_append(optItem, emuOptions[i].option);
		}

		if (emuOptions[i].type == EMU_OPT_INT) {
			ap_add_int_opt(parser, optItem->str, 0);
		} else {
			ap_add_str_opt(parser, optItem->str, "");
		}

		g_string_free(optItem, true);

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

const char *emulatorOptionString(const char *name)
{
	int i;

	if (ap_count(parser, name)) {
		return ap_get_str_value(parser, name);
	}

	i = 0;
	while (emuOptions[i].option != NULL) {
		if ((strcmp(emuOptions[i].option, name) == 0)
			&& (emuOptions[i].type == EMU_OPT_CHAR)
			&& (emuOptions[i].charVal != NULL)) {
			return emuOptions[i].charVal;
		}
		i++;
	}

	return "";
}

int emulatorOptionInt(const char *name)
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

int emulatorOptionDevCount(const char *name) {
	int i, count;

	count = ap_count(parser, name);

	i = 0;
	while (emuOptions[i].option != NULL) {
		if (strcmp(emuOptions[i].option, name) == 0) {
			if (emuOptions[i].list != NULL) {
				count += g_list_length(emuOptions[i].list);
			}
		}

		i++;
	}

	return count;
}

const char *emulatorOptionDev(const char *name, int idx)
{
	int i = 0;

	if (idx < ap_count(parser, name)) {
		return ap_get_str_value_at_index(parser, name, idx);
	}

	idx -= ap_count(parser, name);

	while (emuOptions[i].option != NULL) {
		if (strcmp(emuOptions[i].option, name) == 0) {
			if (emuOptions[i].list != NULL) {
				return g_list_nth_data(emuOptions[i].list, idx);
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

char *emulatorOptionArgv(int idx)
{
	return ap_get_arg_at_index(parser, idx);
}
