# ceda-cemu

An emulator for Sanco 8000, a Z80-based French computer.
This time, written in plain old C.

## Build
```
git submodule init
git submodule update
script/build release
```

## Run
Place a copy of BIOS and Character ROM inside `rom/`, then:
```
build/release/ceda
telnet 127.0.0.1 52954
```

Emulation can be started/stopped/resumed via the provided command line debugger.

In case you are wondering, *52954* is just the decimal version of `0xCEDA`.

## Development
- to add debug symbols:
```
script/build debug
```

- to compile tests
```
script/build test
```

- to run tests:
```
build/test/ceda
```

### Script
The `script/` directory contains some useful script for development.
- `format`: clang-format sources
- `valgrind`: check for memory leaks

## About
This emulator is part of a documentation effort by [Retrofficina GLG Programs](https://retrofficina.glgprograms.it/). See [ceda-home](https://github.com/GLGPrograms/ceda-home).
