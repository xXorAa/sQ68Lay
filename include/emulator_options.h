#pragma once

#ifndef EMULATOR_OPTIONS_H
#define EMULATOR_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

int emulatorOptionParse(int argc, char **argv);
void emulatorOptionsRemove(void);
const char *emulatorOptionString(const char *name);
int emulatorOptionInt(const char *name);
int emulatorOptionDevCount(const char *name);
const char *emulatorOptionDev(const char *name, int idx);
int emulatorOptionArgc(void);
const char *emulatorOptionArgv(int idx);

#ifdef __cplusplus
};
#endif

#endif /* EMULATOR_OPTIONS_H */