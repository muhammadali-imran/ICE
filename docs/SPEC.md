# ICE Specification

## Purpose and status

ICE is a C++23 chess engine project intended to grow into an engine that plays
sound tactical chess. The current implementation is the rules and move
generation foundation: FEN input/output, board inspection, legal move
generation, make/unmake, and perft. It does **not** yet implement UCI, search,
or position evaluation.

This document preserves the original ICE v0 goal, decisions, requirements,
toolchain, and project standards, while labeling items that remain planned
instead of presenting them as implemented. Detailed target search and
evaluation design is in [ENGINE-DESIGN.md](ENGINE-DESIGN.md); detailed
acceptance checks are in [TEST_CASES.md](TEST_CASES.md). Future milestones and
proposals are in [ROADMAP.md](ROADMAP.md).

## Goal and success criteria

The long-term goal is a C++23 chess engine that plays legal, tactically sound
chess and can hold its own against a 1500–2000-rated human in a rapid game. It
does not need to compete with Stockfish or commercial chess engines. The
rating target is approximate, so the original plan defines these proxies:

| ID | Success measure | Target |
|---|---|---|
| G1 | Correctness | Perft counts match the references in [TEST_CASES.md](TEST_CASES.md) exactly. |
| G2 | Reliability | Full UCI games without illegal moves, crashes, hangs, or time forfeits. |
| G3 | Tactical strength | Avoids one- to two-ply piece blunders and finds mates in one to three. |
| G4 | Calibration | Clear majority in 100+ games against Stockfish limited near 1500, then near 1800–2000. |
| G5 | Human check | Holds its own in rapid play, such as 10+0 or 15+10, against the intended player. |

Stockfish's `UCI_Elo` setting is only a rough calibration ruler; it is not
directly comparable to chess.com or FIDE ratings. Playing the engine remains
the practical check.

These are project goals, not current performance claims. At present, only
the move-generation foundation and its correctness tests exist.

## Non-goals

The original v0 scope excluded multithreading, opening books, endgame
tablebases, pondering, neural-network evaluation, a custom GUI, Chess960,
native Windows and macOS builds, and integration with other projects or AI
tooling. The [ROADMAP.md](ROADMAP.md) records later candidates; these exclusions
are not commitments to implement them.

## Current behavior

- Uses 64-bit bitboards and little-endian rank-file squares (`a1 = 0`,
  `h8 = 63`).
- Parses FEN and prints normalized FEN; optional halfmove and fullmove
  counters default to `0` and `1`.
- Supports standard chess move rules, including castling, en passant,
  promotions, and filtering moves that leave the king in check.
- Tracks castling rights, en-passant square, clocks, and a Zobrist position
  key across make/unmake.
- Provides command-line `fen`, `perft`, and `divide` commands.
- Uses ray-loop sliding attacks and compile-time leaper attack tables.

The command syntax and build instructions are in [SETUP.md](SETUP.md).

## Current command contract

```text
ice perft <depth> [fen]
ice divide <depth> [fen]
ice fen [fen]
```

Without a FEN, commands use the standard starting position. A FEN may be
passed as one quoted argument or as separate trailing arguments. Perft depth
is accepted from 0 through 20. Invalid commands or depth values print usage or
an error and return a non-zero exit status.

## Project constraints

- Language: standard C++23, without compiler extensions.
- Build: CMake 3.20 or newer; Ninja is the documented generator.
- Compilers documented by the project: GCC 13+ and Clang 18+.
- Engine core runtime dependencies: C++ standard library only.
- Catch2 v3.16.1 is fetched by CMake for tests.
- Debug builds enable AddressSanitizer and UBSan by default.
- Ubuntu is the documented supported development platform. Native Windows
  builds are not currently supported; use WSL for Windows development.

## Functional requirements (target)

The following requirements preserve the original complete-engine target.
Requirements marked "planned" are not implemented merely by being listed here.

| ID | Requirement | Current status |
|---|---|---|
| FR1 | Parse and print positions in FEN; round-trip supported positions exactly. | Implemented for the current parser contract. |
| FR2 | Generate every legal move, including castling, en passant, all four promotions, and check evasions. | Implemented and covered by movegen/perft tests. |
| FR3 | Make and unmake moves while correctly updating castling, en-passant state, halfmove clock, and Zobrist key. | Implemented and tested. |
| FR4 | Detect checkmate, stalemate, fifty-move draws, threefold repetition, and insufficient material in game play and search. | Partially implemented: checkmate/stalemate positions are exercised through legal-move exhaustion; draw adjudication is not implemented. |
| FR5 | Search under a time budget using iterative deepening, alpha-beta, move ordering, quiescence, and a transposition table. | Planned. |
| FR6 | Evaluate positions in centipawns from the side-to-move perspective. | Planned. |
| FR7 | Communicate over UCI. | Planned. |
| FR8 | Provide a `perft` command for testing. | Implemented. |

## Non-functional requirements (target)

| ID | Requirement | Current status |
|---|---|---|
| NFR1 | Correctness before speed; do not optimize move generation before perft passes. | Active project rule. |
| NFR2 | Fixed-depth, single-threaded searches are deterministic: same position yields the same move. | Applies when search is implemented. |
| NFR3 | No undefined behavior under AddressSanitizer and UBSan across tests. | Debug sanitizer configuration exists; run it for validation. |
| NFR4 | Configurable hash table size through UCI `Hash`, default 16 MB. | Planned with UCI/search. |
| NFR5 | No runtime dependencies beyond the C++ standard library. | Current core follows this; Catch2 is test-only. |
| NFR6 | Build in a few commands on Ubuntu with GCC or Clang. | Current documented workflow. |
| NFR7 | Time management must leave a safety margin to avoid time forfeits. | Planned with search. |

## Decisions log

These are the original design decisions and their rationale. Status describes
whether the decision is in use today, or remains a target.

| ID | Decision | Rationale and status |
|---|---|---|
| D1 | Language: C++. | Familiar language; native 64-bit integers suit bitboards without a new language. In use. |
| D2 | Standard: C++23. | Requested beyond C++20. The original research reported that stock Ubuntu 24.04 compilers (GCC 13.3 and Clang 18.1.3) rejected `-std=c++26` at that time, and that reflection, contracts, and `std::execution` offered no clear engine benefit. This is historical rationale, not a current claim about C++26 support. In use. |
| D3 | Board representation: bitboards. | Efficient attack queries and useful for learning bitboard techniques. In use. |
| D4 | Little-endian rank-file mapping (`a1 = 0`, `sq = rank * 8 + file`). | One consistent convention, used by common engine references. In use. |
| D5 | Precomputed knight/king attacks and pawn shifts/tables. | Simple and fast; tables can be `constexpr`. Current code uses compile-time leaper and pawn tables. |
| D6 | Ray loops for rook, bishop, and queen attacks. | Straightforward reference implementation; faster magic tables can later be checked against it. In use. |
| D7 | Pseudo-legal moves followed by a make/check legality filter. | Simpler first correct implementation than legal-by-construction generation. In use. |
| D8 | Make/unmake with an undo record for board and metadata. | Avoids copying positions during recursive work and restores castling, en-passant, clocks, and hash state. In use. |
| D9 | Packed integer move representation (16 bits where sufficient). | Compact for storage, sorting, and future transposition-table use. In use as 16-bit moves. |
| D10 | UCI protocol. | Supported by chess GUIs and match tools. Planned; no UCI loop yet. |
| D11 | Negamax alpha-beta, iterative deepening, time control, quiescence, hash/PV and heuristic move ordering, and Zobrist-keyed transposition table. | Conventional search design intended to meet the strength goal. Planned; see [ENGINE-DESIGN.md](ENGINE-DESIGN.md). |
| D12 | Hand-written tapered evaluation: material, piece-square tables, bishop pair, pawn structure, king safety, and mobility. | Intended to reach the project target without neural evaluation. Planned; see [ENGINE-DESIGN.md](ENGINE-DESIGN.md). |
| D13 | CMake 3.20+ with Ninja as the documented generator. | CMake version supports the C++23 standard selection used by the project. In use. |
| D14 | Standard library only in the engine; Catch2 only for tests. | Keeps runtime dependencies small and chess logic self-written. In use. |
| D15 | Perft as primary move-generation correctness test, then unit tests and engine matches. | Published node counts provide exact move-tree checks. In use for current scope. |
| D16 | AddressSanitizer and UBSan in Debug builds. | Detects invalid shifts, indexing, and arithmetic errors. Enabled by default in the current Debug configuration. |
| D17 | Use a baseline-safe C++23 subset supported by GCC 13 and Clang 18. | Keeps the project buildable on the documented Ubuntu 24.04 toolchain. Verify compiler support when adopting newer features. |
| D18 | GitHub hosting and Ubuntu development/testing. | Original project platform and hosting choice. Ubuntu remains the documented target. |

## Project standards

### Language and compiler

- Compile as standard C++23 on GCC 13+ and Clang 18+.
- Use `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`; fix warnings rather
  than suppressing them without justification.
- Disable compiler extensions with `CMAKE_CXX_EXTENSIONS OFF`.
- Use `<bit>` operations such as `std::popcount`, `std::countr_zero`, and
  `std::bit_width` instead of compiler-specific bit intrinsics in core code.
- Confirm the supported compiler set if the compiler floor or language subset
  changes.

### Code conventions

| Rule | Purpose |
|---|---|
| Use `std::uint64_t` for bitboards and scoped enums for chess concepts. | Avoid implicit conversions and make representations explicit. |
| Never shift by a possibly out-of-range amount, including 64 or more. | Avoid undefined behavior. |
| Avoid signed/unsigned comparisons; make conversions explicit and explain non-obvious ones. | Prevent subtle arithmetic and bounds bugs. |
| Avoid heap allocation, exceptions, and virtual dispatch on the future search hot path. | Keep recursive search predictable. |
| Make tables `constexpr` or initialize them once. | Keep lookup behavior predictable and thread-safe. |
| Keep one small public header per module and keep core logic free of I/O. | Preserve testability and clear boundaries. |
| Assert debug invariants such as valid square indices. | Fail close to invariant violations. |
| Types use `PascalCase`; functions and variables use `snake_case`; constants use `kPascalCase`; avoid macros. | Consistent naming. |
| Comment the reason for non-obvious chess rules or bit tricks, not obvious operations. | Keep comments explanatory. |

### Definition of done

For any code change:

1. Build without warnings on GCC and Clang in Debug and Release when those
   toolchains are available.
2. Pass the test suite, including sanitizer-enabled validation.
3. Preserve perft references whenever move generation or board transitions
   change.
4. Test strength-affecting changes against the previous version rather than
   assuming they improve play.
5. Update this specification when a decision or requirement changes. Update
   C++ compatibility notes when compiler behavior is discovered.

For documentation-only changes, validate references and accuracy; code tests
are not required unless a documentation test exists.

### Version-control workflow

The original workflow expects `main` to build and pass tests, short-lived
branches, small commits focused on one concern, and version tags whose release
notes include perft and match results. Generated `build/` output is ignored.
These are project conventions, not an instruction for an agent to create
commits unless explicitly asked.

## Repository layout history

The original spec anticipated future `search.cpp`, `eval.cpp`, `tt.cpp`, and
`uci.cpp` modules and match scripts under `tools/`. The current repository map,
which supersedes that aspirational sketch for present-day paths, is in
[STRUCTURE.md](STRUCTURE.md).

## Toolchain and development tools

The currently documented requirements are Ubuntu, C++23, GCC 13+ or Clang
18+, CMake 3.20+, Ninja, Git, and internet access for the first test-enabled
CMake configure (Catch2 is fetched with `FetchContent`). The original
specification recorded Ubuntu 24.04.5, GCC 13.3, Clang 18.1.3, CMake 3.28.3,
Ninja 1.11.1, and Git 2.43 as the versions it checked. These are historical
observations, not a guarantee that they are the latest versions. On Ubuntu,
the original package suggestion is:

```bash
sudo apt install build-essential cmake ninja-build gdb git clang
```

The implementation's `CMakeLists.txt` is authoritative for current targets
and options. The older specification proposed this general setup:

- Catch2 v3.16.1 for unit tests.
- AddressSanitizer and UBSan for Debug.
- gdb for debugging.
- cutechess-cli for future engine matches (the original research noted
  v1.5.1 and Qt 6.8+ as a source-build requirement), Cute Chess as a Linux UCI
  GUI, and Stockfish for strength calibration once those capabilities exist.

For current build commands and the Clang sanitizer-runtime note, see
[SETUP.md](SETUP.md). Do not infer support for a match tool or protocol from
its appearance in this specification.

The intended engine avoids Boost, threads, SIMD intrinsics, `import std`,
neural-network libraries, and GUI toolkits in the core. Some of these remain
possible future work, not current dependencies.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Subtle move-generation errors in castling, en passant, promotions, or pins. | Run the full reference perft set, including mirrored Position 4, and targeted edge cases. |
| Undefined behavior from shifts, indexing, and mixed integer types. | Run sanitizer-enabled Debug tests and retain conversion warnings. |
| A strength-related change quietly weakens the engine. | Compare versions in controlled engine matches once search is implemented. |
| GCC/Clang differences. | Keep to the baseline-safe subset and build with both supported toolchains. |
| Human strength target is imprecise. | Use perft, tactical tests, engine matches, and human play as separate proxies. |
| Scope creep into neural evaluation, threads, books, or integrations. | Keep these as roadmap proposals until explicitly selected. |

## Open items and historical notes

- **License:** the original spec listed this as undecided. A `LICENSE` file is
  now present in the repository; verify its contents and intended terms before
  making a release.
- **Repository visibility/name:** the original spec left visibility open.
  Repository identity is now established by the hosting location, but public
  release policy remains a maintainer decision.
- **C++23 tracker:** the original companion tracker is not present in the
  current repository. Record compiler-specific findings in an appropriate
  maintained document if that knowledge becomes necessary.

## References

The following sources appeared in the original specification. Its "opened",
"listed", and "not opened" labels describe the research status at the time the
original document was written, not a fresh verification.

| Topic | Source | Original research note |
|---|---|---|
| C++23 features | [cppreference: C++23](https://en.cppreference.com/w/cpp/23) | Opened; the first 100,000 of 114,434 characters were reviewed. |
| Compiler support | [cppreference: C++23 compiler support](https://en.cppreference.com/w/cpp/compiler_support/23) | Opened. |
| C++26 status | [Wikipedia: C++26](https://en.wikipedia.org/wiki/C%2B%2B26) | Opened for the historical decision rationale. |
| CMake standard property | [CMake: `CXX_STANDARD`](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html) | Opened. |
| Test framework | [Catch2](https://github.com/catchorg/Catch2) | Opened; v3.16.1 was checked at the time. |
| Match runner | [Cute Chess](https://github.com/cutechess/cutechess) | Opened; v1.5.1 was checked at the time. Exact UCI match flags were not verified in the original research. |
| Reference engine | [Stockfish](https://github.com/official-stockfish/Stockfish) and its [UCI wiki](https://github.com/official-stockfish/Stockfish/wiki/UCI-&-Commands) | Main project page opened; UCI wiki page failed to load at that time. |
| UCI background | [Chess Programming Wiki: UCI](https://www.chessprogramming.org/UCI) | Opened. |
| UCI specifications | [Shredder Chess UCI overview](https://www.shredderchess.com/chess-features/uci-universal-chess-interface.html) and [WBEC UCI protocol](http://wbec-ridderkerk.nl/html/UCIProtocol.html) | Listed, not verified in the original research. |
| Magic bitboards | [Chess Programming Wiki: Magic Bitboards](https://www.chessprogramming.org/Magic_Bitboards) | Opened. |
| Perft references | [Chess Programming Wiki: Perft Results](https://www.chessprogramming.org/Perft_Results) | Opened. |
| Evaluation starting values | [Chess Programming Wiki: Simplified Evaluation Function](https://www.chessprogramming.org/Simplified_Evaluation_Function) | Not opened; values were based on prior knowledge in the original spec. |
| Original draft material | `chess engine idea.md` and an uploaded magic-bitboard article | Read according to the original document; these source files are not maintained here. |

## Original specification change history

| Date | Change |
|---|---|
| 2026-10-09 | Draft 1, written by Claude. |
| 2026-10-10 | Reviewed edits; added project scope, platform, project name, GitHub, search/evaluation design; moved future items to a companion roadmap; corrected tables and cross-references; marked final. |
| 2026-10-24 | Reorganized into the current docs set, restoring the goals, decisions, requirements, toolchain, standards, risks, open items, and references while distinguishing target behavior from implemented behavior. |

## Design decisions

- Bitboards use `std::uint64_t`; typed `enum class` values represent core
  chess concepts.
- Knight, king, and pawn attacks use precomputed tables; sliders use ray
  walks as a clear reference implementation.
- Pseudo-legal generation followed by make/check/unmake is used for legality.
- Board updates use a fixed undo history and preserve Zobrist state.
- The core contains no command-line or protocol I/O.
- Correctness is prioritized over optimization; perft results must remain
  unchanged when move generation is modified.

## Planned, not yet implemented

The wider project goal includes a UCI interface, search with time management,
evaluation, draw handling, and strength calibration. Those are roadmap goals;
do not assume they are available or describe them as current behavior.
The roadmap also records later candidates such as faster slider attacks,
opening books, tablebases, and multi-threading.

## Related documents

- [ARCHITECTURE.md](ARCHITECTURE.md): component interactions and invariants.
- [STRUCTURE.md](STRUCTURE.md): repository files and responsibilities.
- [TEST_CASES.md](TEST_CASES.md): implemented coverage and test commands.
- [ROADMAP.md](ROADMAP.md): milestones and possible future improvements.
- [SETUP.md](SETUP.md): build and command-line usage.
