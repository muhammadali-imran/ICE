# ICE setup and usage

This guide covers the supported development setup for ICE, how to build and test
it, and how to run the commands currently available in the engine.

## Requirements

- Ubuntu Linux (the project's documented development platform)
- A C++23 compiler: GCC 13 or newer, or Clang 18 or newer
- CMake 3.20 or newer
- Ninja
- Git and an internet connection for the first configure, which downloads Catch2
  v3.16.1 for the tests

The engine itself uses only the C++ standard library. Catch2 is only required
for building and running tests.

On Ubuntu, install the build tools with:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build clang git
```

Check that the tools are available and that the compiler meets the version
requirement:

```bash
g++ --version
cmake --version
ninja --version
git --version
```

Use `clang++ --version` instead if you intend to build with Clang.

## Build and test

Run these commands from the repository root.

For a Debug build, which enables AddressSanitizer and UBSan by default:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

For an optimized Release build, including the registered slow perft tests:

```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

If using Clang without its sanitizer runtime installed, disable sanitizers for
the Debug build by adding `-DICE_SANITIZERS=OFF` to the first CMake command.

The build places the engine and its tests at `build/debug/ice` and
`build/debug/ice_tests` (or under `build/release/` for a Release build).

## Run the engine

The current version does not yet implement UCI or a graphical interface. It
provides command-line utilities for perft counts, perft divide, and FEN
inspection.

From the repository root, use the executable from the build directory. For
example, with a Release build:

```bash
build/release/ice perft 5
build/release/ice divide 3
build/release/ice fen
```

`perft` counts legal move sequences to the requested depth. `divide` prints
the count for each legal root move. `fen` prints the board and normalized FEN.
Each command uses the standard starting position when no FEN is supplied.

To use a custom FEN, pass its fields after the command. Quote the FEN to keep
it as one shell argument:

```bash
build/release/ice perft 4 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
build/release/ice fen "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
```

Depth must be between 0 and 20. The command prints usage when invoked without
a command or with invalid arguments.

## Windows

Native Windows builds are not currently a supported project target. On
Windows, use Ubuntu through WSL and follow the Linux instructions above from
the WSL terminal.
