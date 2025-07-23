# Dependencies

```
SDL3
```

# Building

```
cmake -B build
cmake --build build
```

# Running
## Log Messages

Logging is controlled by the SDL_LOGGING variable for example to silence all logs

```
SDL_LOGGING=app=quiet ./build/sqlay3
```

or to enable debug messages

```
SDL_LOGGING=app=debug ./build/sqlay3
```

## sq68ux

```
./build/sq68ux --help

Usage: sq68ux [OPTIONS] [args...]

Positionals:
  args                        Arguments passed to QDOS

Options:
  -h,--help                   Print this help message and exit
  -f,--CONFIG [sq68ux.ini]    Read an ini file
  --smsqe                     smsqe image to load (at 0x32000)
  --sd1                       SDHC Image for SD1 slot
  -r,--sysrom                 system rom to load (at 0x0)
  --trace [0]                 enable tracing
  --version                   version number
```

## sqlay3

```
./build/sqlay3 --help

Usage: sqlay3 [OPTIONS] [args...]

Positionals:
  args                        Arguments passed to QDOS

Options:
  -h,--help                   Print this help message and exit
  -f,--CONFIG [sqlay3.ini]    Read an ini file
  -m,--ramsize [128]          amount of ram in K (max 8192)
  -c,--exprom                 address@romfile eg C000@NFA.rom
  -r,--sysrom [JS.rom]        system rom
  -l,--drive                  nfa file (upto 8 times)
  --trace [0]                 enable tracing
  --version                   version number
```

