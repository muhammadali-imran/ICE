# ICE

A chess engine written in C++23. The goal is an engine that plays sound, tactical chess and can beat a 1500 to 2000 rated human in a rapid game.

**Status:** ICE v0 is in progress. Milestones v0.1 (types, bitboards, FEN) and v0.2 (attack tables, move generation, make/unmake, perft) are done. UCI, search and evaluation come next. See [docs/ICE-FUTURE-IMPROVEMENTS.md](docs/ICE-FUTURE-IMPROVEMENTS.md) for the milestone list.

## Documents

See [docs/setup.md](docs/setup.md) for requirements, build and test instructions, and current run commands.

| File | What it is |
|---|---|
| [docs/v0/ICE-V0-SPEC.md](docs/ICE-V0-SPEC.md) | What ICE v0 is: goals, decisions, standards, requirements, acceptance tests |
| [docs/v0/ICE-FUTURE-IMPROVEMENTS.md](docs/ICE-FUTURE-IMPROVEMENTS.md) | Milestones and everything planned after v0 |

## Build and test

Needs a C++23 compiler (GCC 13 or Clang 18), CMake 3.20 or newer, and Ninja. On Ubuntu:

```bash
sudo apt install build-essential cmake ninja-build clang
```

Debug build (AddressSanitizer and UBSan on) and the fast tests:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Release build, including the depth-5 perft tests:

```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

Clang's sanitizer runtime is a separate package on Ubuntu. If you build with Clang and do not have it, add `-DICE_SANITIZERS=OFF` to the debug configure command.

The first configure downloads Catch2 (v3.16.1) with CMake's `FetchContent`, so it needs network access.

Perft depth 6 (about a minute and a half in a release build) is hidden by default:

```bash
build/release/ice_tests "[.deep]"
```

## Developer commands

Until the UCI loop exists (v0.3) the `ice` program only has these commands:

```bash
ice perft <depth> [fen]    count legal move sequences (default: start position)
ice divide <depth> [fen]   perft with a count under each root move
ice fen [fen]              print the board and the normalized FEN
```

Example: `build/release/ice perft 5` prints `nodes: 4865609`.

## Source layout

| File | Purpose |
|---|---|
| `src/types.h` | Colors, pieces, squares, castling rights |
| `src/bitboard.h` | Bitboard helpers (popcount, lowest set bit, shifts, masks) |
| `src/move.h` | 16-bit packed move |
| `src/zobrist.h` | Zobrist hash keys, generated at compile time |
| `src/attacks.h`, `attacks.cpp` | Knight, king and pawn tables; ray-loop rook, bishop and queen attacks |
| `src/board.h`, `board.cpp` | Position, FEN, make/unmake, attack queries |
| `src/movegen.h`, `movegen.cpp` | Pseudo-legal and legal move generation |
| `src/perft.h`, `perft.cpp` | Perft and divide |
| `src/main.cpp` | Command line entry point |
| `tests/` | Catch2 tests: moves, attacks, board and FEN, perft against the reference values |
