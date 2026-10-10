# ICE: Chess Engine Specification (ICE v0)

| | |
|---|---|
| **Document** | ICE-V0-SPEC.md |
| **Project** | ICE |
| **Version covered** | ICE v0 |
| **Status** | Final |
| **Date** | 2026-10-10 |
| **Scope** | The engine and the game board (position representation and chess rules). No custom GUI in v0: ICE is played through an existing UCI program. |
| **Platform** | Ubuntu (Linux) |
| **Code hosting** | GitHub |
| **Supersedes** | `chess engine idea.md` (the first rough plan). Its search and evaluation design is carried into sections 7.3 and 7.4. |
| **Companions** | [ICE-FUTURE-IMPROVEMENTS.md](ICE-FUTURE-IMPROVEMENTS.md) for milestones and everything after v0. [ICE-CPP23-TRACKER.md](ICE-CPP23-TRACKER.md) for the running record of C++23 findings. |

This document describes **only what ICE v0 is and does**. Anything planned for later lives in the companion documents.

---

## 1. Goal

Build a chess engine in **C++23** that plays legal, tactically sound chess and is strong enough to **beat a 1500 to 2000 rated human in a rapid game**. It does not need to compete with Stockfish or chess.com engines.

### 1.1 What "strong enough" means (measurable)

The human-rating target is approximate and cannot be measured directly, so we use proxies:

| # | Proxy | Target for v0 |
|---|---|---|
| G1 | Correctness | Perft node counts match the reference values in section 8.1 exactly |
| G2 | Reliability | Plays full games over UCI with no illegal moves, crashes, hangs or lost-on-time games |
| G3 | Tactical strength | Never hangs pieces to 1 to 2 ply tactics; finds mates in 1 to 3 |
| G4 | Calibration | Wins a clear majority of a 100+ game match against Stockfish with its strength limit set near 1500, then near 1800 to 2000 |
| G5 | Human check | You play it yourself in rapid (10+0 or 15+10) and it holds its own at your level |

Caveat for G4: Stockfish's `UCI_Elo` scale is calibrated for its own conditions and is **not** the same scale as chess.com or FIDE ratings. Treat it as a rough ruler. G5 is the check that actually matters.

### 1.2 Non-goals for v0

Multithreading, opening books, endgame tablebases, pondering, neural-network evaluation, a custom GUI, Chess960, Windows and macOS builds, and any integration with other projects or AI tooling.

---

## 2. Decisions log

Each row is a decision we have taken, and why.

| ID | Decision | Rationale |
|---|---|---|
| D1 | **Language: C++** | You already write it. Native 64-bit integers suit bitboards, and there is no new language to learn alongside chess programming. Python is too slow for search, and JavaScript `BigInt` is slow. |
| D2 | **Standard: C++23** | You asked for a newer standard than C++20. C++26 was finalized in March 2026 and GCC 16.1 supports most of it, but the stock compilers on Ubuntu 24.04 (GCC 13.3, Clang 18.1.3, as tested) reject `-std=c++26`, and its headline features (reflection, contracts, `std::execution`) do nothing for an engine. |
| D3 | **Board: bitboards** | You asked to learn them. Attack queries and evaluation terms become single bitwise operations. |
| D4 | **Square mapping: little-endian rank-file** (`a1 = 0 … h8 = 63`, `sq = rank*8 + file`) | One convention used everywhere, and the one most tutorials and the magic-bitboard references use. |
| D5 | **Knights, kings: precomputed tables. Pawns: shifts with file masks.** | Simple and fast. Tables can be built at compile time with `constexpr`. |
| D6 | **Sliders (rook, bishop, queen): ray loops** | Easy to get right. Magic bitboards come later, generated from and checked against the ray loops (section 7.2). |
| D7 | **Legality: pseudo-legal generation plus a king-in-check filter** | Easiest to get correct first. The behaviour is identical to generating only legal moves. Only the implementation differs. |
| D8 | **Make/unmake with an undo stack** (incremental update of boards, castling rights, en passant, halfmove clock, Zobrist key) | Faster than copying the board and a natural fit for recursive search. |
| D9 | **Move encoding: packed integer** (from, to, flags in 16 bits, optionally 32) | Cheap to store, sort, and put in the transposition table. |
| D10 | **Protocol: UCI** | Simplest, supported by every GUI and match tool, and what Stockfish speaks. |
| D11 | **Search:** negamax alpha-beta, iterative deepening with time control, quiescence search, move ordering (hash/PV move, MVV-LVA, killers, history), Zobrist-keyed transposition table (section 7.3) | Standard and sufficient for the target. |
| D12 | **Evaluation:** material, piece-square tables (tapered between middlegame and endgame), bishop pair, pawn structure, king safety, mobility. Hand-written (section 7.4). | Hand-written evaluation reaches the target. Neural evaluation is not used. |
| D13 | **Build: CMake (3.20 or newer) with Ninja** | CMake 3.20 is the first version that knows `CXX_STANDARD 23`. |
| D14 | **Dependencies: standard library only** in the engine. Catch2 only for tests. | Trivial builds, and you write the chess logic yourself. |
| D15 | **Testing: perft as the primary correctness test**, then unit tests, then engine-vs-engine matches | Perft either matches the known counts or it does not. |
| D16 | **Development safety: AddressSanitizer and UBSan in debug builds** | Catches shift-by-64, out-of-range indexing and signed overflow early. |
| D17 | **Baseline-safe C++23 subset** (see the tracker, section 3) | Keeps the project buildable on stock Ubuntu 24.04 compilers (GCC 13, Clang 18) until we deliberately raise the floor. |
| D18 | **Code lives on GitHub; development and testing on Ubuntu** | Your platform and hosting choices. |

---

## 3. Project standard

These are the rules we hold the code and the process to. They are proposals that can change.

### 3.1 Language and compiler policy

- The code must compile cleanly with `-std=c++23` on **GCC 13+** and **Clang 18+**. If a feature does not compile on both, it is not in the baseline-safe subset (tracker, section 3).
- Dev builds use `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`. Warnings are fixed, not ignored.
- No compiler extensions. `CMAKE_CXX_EXTENSIONS OFF`.
- Bit operations only through `<bit>` (`std::popcount`, `std::countr_zero`, `std::bit_width`). No raw `__builtin_*` in the core.

### 3.2 Code rules

| Rule | Why |
|---|---|
| Use `std::uint64_t` for bitboards and `enum class` for `Color`, `PieceType`, `Square`. | Avoids implicit conversions. |
| Never shift by an amount that can be 64 or more. Guard it or mask it. | Undefined behaviour. |
| Avoid signed and unsigned comparisons. Use explicit casts with a comment. | Subtle bugs. `-Wconversion` helps. |
| No heap allocation, exceptions or virtual calls on the search path. | Predictable speed. |
| Tables are `constexpr` or built once at startup. | Faster and thread-safe. |
| One header per module with a small public surface. Core logic has no I/O. | Testable. |
| Assertions (`assert`) in debug builds for invariants such as "square is 0..63". | Fail early. |
| Naming: types `PascalCase`, functions and variables `snake_case`, constants `kPascalCase`, macros avoided. | One style. |
| Comment the *why* (chess or bit trick), not the *what*. | You are learning from this code. |

### 3.3 Definition of done (for any change)

1. Builds with no warnings on GCC and Clang, debug and release.
2. All tests pass, including under sanitizers.
3. Perft values still match section 8.1 (whenever move generation is touched).
4. Any change that could alter playing strength was tested in a head-to-head match against the previous version, not assumed.
5. This document is updated if a decision or requirement changed. The tracker is updated if we learned something about C++23.

### 3.4 Repository layout

```txt
ICE/
  docs/     ICE-V0-SPEC.md  ICE-FUTURE-IMPROVEMENTS.md  ICE-CPP23-TRACKER.md
  src/      types.h  bitboard.h  attacks.cpp  board.cpp  movegen.cpp
            search.cpp  eval.cpp  tt.cpp  uci.cpp  main.cpp
  tests/    perft_test.cpp  board_test.cpp  ...
  tools/    run_match.sh  (cutechess-cli wrappers)
  CMakeLists.txt
  README.md  LICENSE  .gitignore
```

### 3.5 Version control (GitHub)

- Git, hosted on GitHub. `main` always builds and passes the tests.
- Work happens on short-lived branches and is merged into `main` only when the definition of done is met.
- Small commits, one concern each, with a message that says why.
- Tag releases `v0.1`, `v0.2`, and so on. Each tag records its perft results and match results in release notes.
- `build/` and other generated files are in `.gitignore`.

---

## 4. Requirements

### 4.1 Functional

| ID | Requirement |
|---|---|
| FR1 | Parse and print positions in FEN. Round-trip must be exact. |
| FR2 | Generate all legal moves, including castling, en passant, promotions (all four pieces) and check evasions. |
| FR3 | Make and unmake moves with correct update of castling rights, en passant, halfmove clock, and Zobrist hashing. |
| FR4 | Detect checkmate, stalemate, the fifty-move rule, threefold repetition, and insufficient material, both in the game and inside search. |
| FR5 | Search with iterative deepening within a time budget, alpha-beta pruning, move ordering, quiescence search, and a transposition table (section 7.3). |
| FR6 | Evaluate positions in centipawns from the side to move (section 7.4). |
| FR7 | Communicate over UCI (section 6). |
| FR8 | Provide a `perft` command for testing. |

### 4.2 Non-functional

| ID | Requirement |
|---|---|
| NFR1 | **Correctness before speed.** Nothing is optimized until perft matches. |
| NFR2 | **Deterministic** with a fixed depth and one thread: same position, same move. |
| NFR3 | Free of undefined behaviour under ASan and UBSan across the test suite. |
| NFR4 | Hash table size is configurable (UCI `Hash` option). Default 16 MB. |
| NFR5 | No runtime dependencies beyond the standard library. |
| NFR6 | Builds with a few commands on Ubuntu with GCC or Clang (section 5.1). |
| NFR7 | Never loses on time by overrunning: time management leaves a safety margin. |

### 4.3 Constraints

- Ubuntu (Linux) is the only supported platform.
- The engine is single-threaded.

---

## 5. Tech stack and tools

### 5.1 Core

Versions below were checked on Ubuntu 24.04.5 LTS.

| Layer | Choice | Minimum version | Notes |
|---|---|---|---|
| Language standard | C++23 (`-std=c++23`) | | CMake chooses the right flag per compiler. |
| Compiler | GCC (`g++`) | 13 | Ubuntu 24.04 ships 13.3. Newer is fine, but only the baseline-safe subset is used. |
| | Clang (`clang++`) | 18 | Ubuntu 24.04 ships 18.1.3. |
| Build system | CMake | **3.20** | `CXX_STANDARD 23` was added in 3.20 (CMake docs). Ubuntu 24.04 ships 3.28.3. |
| Generator | Ninja | any | 1.11.1 on Ubuntu 24.04. |
| Version control | Git (2.43 on Ubuntu 24.04) and GitHub | | |

Install on Ubuntu (package names are standard, but I did not run `apt` in my test environment, so check `g++ --version` afterwards):

```bash
sudo apt install build-essential cmake ninja-build gdb git clang
```

Suggested `CMakeLists.txt` core. This skeleton was built and tested with GCC 13.3 (debug with sanitizers, 1 Catch2 test passing) and with Clang 18.1.3 (release):

```cmake
cmake_minimum_required(VERSION 3.20)
project(ice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug builds: sanitizers. Release builds: optimizations.
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")   # add -march=native for local builds only

add_library(ice_core src/attacks.cpp src/board.cpp src/movegen.cpp)
target_include_directories(ice_core PUBLIC src)
target_compile_options(ice_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

add_executable(ice src/main.cpp src/uci.cpp src/search.cpp src/eval.cpp src/tt.cpp)
target_link_libraries(ice PRIVATE ice_core)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.16.1)
  FetchContent_MakeAvailable(Catch2)
  add_executable(ice_tests tests/perft_test.cpp tests/board_test.cpp)
  target_link_libraries(ice_tests PRIVATE ice_core Catch2::Catch2WithMain)
  add_test(NAME ice_tests COMMAND ice_tests)
endif()
```

Build and test:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Sanitizer note: the GCC debug build with sanitizers worked. A Clang sanitizer build failed to link in my test environment because the Clang sanitizer runtime package was not installed there (it is a separate package on Ubuntu, probably `libclang-rt-18-dev`, which I did not verify). Use GCC for sanitizer builds unless you install that package.

### 5.2 Development and test tools

| Purpose | Tool | Notes |
|---|---|---|
| Unit tests | **Catch2 v3** (pinned to `v3.16.1`, the newest tag at the time of writing) | Needs C++14 or newer. v3 is a compiled library, not single-header. Fetched by CMake `FetchContent`. |
| Sanitizers | AddressSanitizer, UBSan (`-fsanitize=address,undefined`) | Built into GCC and Clang. |
| Debugging | gdb | |
| Match runner | **cutechess-cli** | Command-line tool from the Cute Chess project (GPLv3+). Runs engine-vs-engine matches. Newest non-beta tag: `v1.5.1`. Building from source needs Qt 6.8 or newer and a C++17 compiler, which stock Ubuntu 24.04 packages may not provide, so prefer a prebuilt binary if one is offered on the project's releases or wiki. |
| GUI | **Cute Chess** | Free, Linux-native, UCI-capable GUI for playing ICE yourself. |
| Reference engine | Stockfish | For strength calibration with its limit-strength options. Check the `uci` output of your version for the exact option names and Elo range. |

### 5.3 What we are deliberately not using

Boost and other libraries, threads, SIMD intrinsics, `import std`, neural-network libraries, and any GUI toolkit in the engine.

---

## 6. Protocol: UCI

UCI (Universal Chess Interface) was designed by Rudolf Huber and Stefan Meyer-Kahlen and released in November 2000. It is a text protocol over stdin and stdout. It is **stateless**: the GUI sends the full position and the clock every time and acts as arbiter (it declares draws after threefold repetition and so on).

The commands below are the v0 target. This list is from my working knowledge of UCI. Confirm details against the specification (section 11) while implementing.

| Direction | Command | Purpose |
|---|---|---|
| GUI → engine | `uci` | Start UCI mode. Engine replies with `id name ICE`, `id author …`, option lines, and `uciok`. |
| | `isready` | Engine replies `readyok` when ready. |
| | `ucinewgame` | Clear hash and history. |
| | `position startpos moves …` / `position fen <fen> moves …` | Set the position. |
| | `go wtime btime winc binc movestogo` | Search with clocks. |
| | `go depth N` / `movetime T` / `nodes N` / `infinite` | Fixed-limit searches for testing. |
| | `stop`, `quit` | Stop searching, exit. |
| | `setoption name Hash value N` | Hash size in MB. |
| Engine → GUI | `info depth … seldepth … score cp … nodes … nps … time … pv …` | Progress during search. |
| | `bestmove e2e4` | The answer. |
| Extra (non-UCI) | `perft N`, `d` (print board) | Debugging, in the style of other engines. |

Design note: parse commands with `std::string_view` and `std::istringstream`. The C++23 `contains` member of strings and string views is handy here.

---

## 7. Techniques and concepts

### 7.1 Bitboards

A bitboard is a `std::uint64_t` where bit *i* says "something is true of square *i*". One integer holds "all white pawns", "squares a knight attacks", or "squares anyone occupies".

| Task | Operation |
|---|---|
| Set, clear, test a square | `bb \|= 1ULL << s`, `bb &= ~(1ULL << s)`, `(bb >> s) & 1` |
| Population count | `std::popcount(bb)` |
| Index of lowest set bit | `std::countr_zero(bb)` |
| Remove lowest set bit | `bb &= bb - 1` |
| Loop over pieces | `while (bb) { s = countr_zero(bb); …; bb &= bb - 1; }` |

Pawn attacks use shifts with file masks to prevent wrap-around from the a-file to the h-file:

```cpp
constexpr std::uint64_t kNotA = 0xfefefefefefefefeULL, kNotH = 0x7f7f7f7f7f7f7f7fULL;
auto white_pawn_attacks = ((pawns << 7) & kNotH) | ((pawns << 9) & kNotA);
auto black_pawn_attacks = ((pawns >> 7) & kNotA) | ((pawns >> 9) & kNotH);
```

Pawn pushes are `(pawns << 8) & ~occupied` for White, and double pushes apply a rank-3 mask to the single-push result.

### 7.2 Sliding pieces

Rooks, bishops and queens depend on blockers, so attacks cannot come from a fixed table indexed by square alone.

**In v0: ray loops.** Walk each direction until a blocker. Slow, obviously correct. They are kept forever as the reference implementation.

**What magic bitboards are** (the technique we adopt after v0, and the reason the ray loops matter). For each square:

1. Mask the relevant occupancy (the squares on the piece's rays, **excluding the board edges**, because a blocker on the edge does not change what is attacked).
2. Multiply by a square-specific 64-bit **magic number**.
3. Shift right by `64 - N`, where *N* is the number of relevant bits.
4. Use the result as an index into a precomputed attack table.

```cpp
index   = ((occupancy & mask[sq]) * magic[sq]) >> (64 - bits[sq]);
attacks = table[sq][index];
```

Is this a "bit permutation"? Loosely. The multiply scatters the masked bits so that they land in the top *N* bits, and the shift then packs them into a dense index. It is really a **perfect hash**: every distinct set of relevant blockers that has a different attack set must get a different index. Different blocker sets that have the *same* attack set may share an index, which is called a constructive collision and is what keeps the tables small.

**Finding the magic numbers.** Not derivable analytically. Random candidates (few set bits works best) are tried, a table is filled for every subset of the mask, and a candidate is rejected if two subsets with different attacks collide. Subsets are enumerated with the carry-rippler trick: `sub = (sub - mask) & mask`. The reference attack for each subset comes from the ray-loop code.

**Variants** (from the Chess Programming Wiki):

| Variant | Idea | Size |
|---|---|---|
| Plain magic | Fixed table size per square (512 entries for bishops, 4096 for rooks) | about 2 MiB for rooks |
| Fancy magic (Pradu Kannan) | Table size and shift matched to each square's bit count | about 800 KiB for rook and bishop combined |
| Black magic (Volker Annuss, 2017) | ORs the complement of the mask instead of ANDing | about 692 KiB |
| PEXT/BMI2 | Hardware bit extract instead of a multiply | Needs a CPU with BMI2 |

Your uploaded article quotes about 2.3 MB for plain magics and under 840 KB for fancy. The Chess Programming Wiki quotes the figures in the table above. The exact numbers depend on how the tables are laid out, so treat them as orders of magnitude.

### 7.3 Search

Carried over from the original plan (`chess engine idea.md`), which this document supersedes.

```txt
Iterative deepening (depth 1..N)
        │
        ▼
Move ordering (hash/PV move, MVV-LVA, killers, history)
        │
        ▼
Alpha-beta search (with transposition table)
        │ at depth 0
        ▼
Quiescence search ──► static evaluation
```

**Negamax alpha-beta.** Minimax with alpha-beta pruning, written as negamax (one function, scores always from the side to move). Alpha is the best score the side to move is already assured of, beta the best the opponent will allow. When alpha ≥ beta the rest of the node cannot matter and is cut off. In the best case this reduces the work from about b^d to about b^(d/2) nodes, where b is the branching factor and d the depth.

**Iterative deepening.** Search depth 1, then 2, up to N, while checking the time budget. If time runs out, return the best move from the last completed depth. The best move of each iteration is searched first in the next one, which improves pruning.

**Move ordering.** In this order:

1. Hash move from the transposition table (the principal-variation move).
2. Captures sorted by **MVV-LVA** (most valuable victim, least valuable attacker), so pawn-takes-queen is tried before queen-takes-pawn. Promotions are scored with the captures.
3. **Killer moves**: quiet moves that caused a beta cutoff at the same depth in a sibling branch.
4. Remaining quiet moves by **history heuristic**: how often each quiet move caused cutoffs across the search.

**Quiescence search.** At depth 0, keep searching captures and promotions until the position is quiet, to avoid the horizon effect (stopping in the middle of an exchange and misjudging it). The side to move may "stand pat" on the static evaluation instead of capturing.

**Transposition table.** A hash table keyed by a 64-bit **Zobrist** key (an XOR of random numbers, one per piece-on-square, plus castling rights, en passant file and side to move, updated incrementally in make/unmake). Each entry stores the key (or part of it), score, depth, best move and a bound type: exact, lower bound or upper bound. Mate scores are stored relative to the node and adjusted by ply when read back. Size is set by the UCI `Hash` option.

**Time management.** Allocate a fraction of the remaining clock (plus increment) per move, check the clock every few thousand nodes, and always leave a safety margin (NFR7).

### 7.4 Evaluation

Scores are in **centipawns** (1 pawn = 100). The evaluation function returns the score from the side to move's point of view, which is what negamax needs:

```cpp
Eval = S_material + S_position + S_king_safety + S_structure + S_mobility
```

**Material** (`S_material`):

| Piece | Centipawns | Note |
|---|---|---|
| Pawn | 100 | |
| Knight | 320 | |
| Bishop | 330 | Bishop pair bonus: +50 |
| Rook | 500 | |
| Queen | 900 | |
| King | 20000 | Never captured. Large so that losing it dominates. |

**Piece-square tables** (`S_position`): one 64-entry table per piece type and game phase, rewarding good squares.

- Knights: high in the center (d4, d5, e4, e5), penalized on the rim.
- Pawns: rewarded for advancing and for central control.
- Rooks: bonus on open and semi-open files.
- Kings: stay in castled corners in the middlegame, move to the center in the endgame.
- **Tapering:** compute a game phase from the remaining non-pawn material and blend the middlegame and endgame scores by it. Black's tables are the White tables flipped vertically.

**King safety** (`S_king_safety`):

- Pawn shield: penalty when the f, g, h pawns (or a, b, c) in front of a castled king have advanced.
- Attack tropism: penalty for enemy pieces close to or attacking squares next to the king (popcount of enemy attacks on the king zone).
- Castling status: penalty if castling rights are lost without having castled.

**Pawn structure** (`S_structure`):

- Isolated pawns (no friendly pawns on adjacent files): penalty.
- Doubled pawns (several pawns on one file): penalty.
- Passed pawns (no enemy pawns ahead on the same or adjacent files): bonus from a table that grows with the pawn's rank.

**Mobility** (`S_mobility`): per piece, the popcount of its attacked squares that are not occupied by its own pieces, times a small weight per piece type.

Starting values for the tables and weights come from published simple evaluation functions and are then adjusted by head-to-head matches. The values live in `eval.cpp` and are documented there.

### 7.5 Draw detection

Fifty-move rule, threefold repetition and insufficient material must be in v0 (FR4). A search that does not see repetitions will walk into draws it thinks are winning or go for repetitions that lose.

---

## 8. Acceptance tests

ICE v0 is accepted when every check below passes. Milestones along the way, and acceptance for anything after v0, are in the future-improvements document.

### 8.1 Perft reference values

From the Chess Programming Wiki "Perft Results" page. These are node counts of the legal-move tree from the FEN at the given depth. Matching them exactly is the single most valuable test (G1).

| Position | FEN | d1 | d2 | d3 | d4 | d5 | d6 |
|---|---|---|---|---|---|---|---|
| Start | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | 20 | 400 | 8,902 | 197,281 | 4,865,609 | 119,060,324 |
| Kiwipete | `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1` | 48 | 2,039 | 97,862 | 4,085,603 | 193,690,690 | 8,031,647,685 |
| Pos 3 | `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1` | 14 | 191 | 2,812 | 43,238 | 674,624 | 11,030,083 |
| Pos 4 | `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1` | 6 | 264 | 9,467 | 422,333 | 15,833,292 | 706,045,033 |
| Pos 5 | `rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8` | 44 | 1,486 | 62,379 | 2,103,487 | 89,941,194 | |
| Pos 6 | `r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10` | 46 | 2,079 | 89,890 | 3,894,594 | 164,075,551 | 6,923,051,137 |

Notes: the wiki lists Kiwipete without the move counters, and I appended `0 1`. Position 4 also has a mirrored form, `r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1`, with the same counts, which is a good colour-symmetry test. Depth 6 and beyond is slow without optimization. Use d1 to d5 as the routine test and the deeper values as a final check.

### 8.2 Other acceptance checks

| Check | Covers | How | Pass |
|---|---|---|---|
| FEN round-trip | FR1 | Parse and print the perft FENs plus extra edge cases (no castling rights, en passant square, move counters) | Output equals input |
| Zobrist consistency | FR3 | After every make and unmake during perft and test games, compare the incremental key with one computed from scratch | Always equal |
| Draw rules | FR4 | Scripted positions: stalemate, fifty-move, threefold repetition, insufficient material (K vs K, K+B vs K, K+N vs K) | Each is recognized as a draw |
| Determinism | NFR2 | Run the same position at the same depth twice, one thread | Same best move and node count |
| Sanitizers | NFR3 | Whole test suite and perft in the debug build | No reports |
| UCI session | FR7 | Scripted stdin session (`uci`, `isready`, `position`, `go depth`), then load ICE in Cute Chess | Expected replies, and ICE plays in the GUI |
| Tactics | G3 | Set of mate-in-1 to 3 and win-a-piece positions at a fixed depth or time | All solved |
| Reliability | G2, NFR7 | At least 100 games against itself and against Stockfish at a limited strength, at a rapid time control | No illegal moves, crashes, hangs or time forfeits |
| Calibration | G4 | 100+ games against Stockfish with its strength limit near 1500, then near 1800 to 2000 | Clear majority of wins |
| Human check | G5 | You play it in rapid | It holds its own at your level |

---

## 9. Risks

| Risk | Mitigation |
|---|---|
| Subtle movegen bugs (castling rights, en passant, promotion, pins) | Perft on all six positions, plus the mirrored Position 4 |
| Undefined behaviour from bit shifts and type mixing | Sanitizers in every debug build; `-Wconversion` |
| A change quietly weakens the engine | Head-to-head matches, one change at a time |
| Compiler differences between GCC and Clang | Baseline-safe subset; build with both before merging |
| Strength target is vague | Proxies G1 to G5, and playing it yourself |
| Scope creep (neural evaluation, threads, books) | They live in the future-improvements document until v0 is accepted |

---

## 10. Open items

1. **License.** The repository needs a `LICENSE` file before it is made public on GitHub. Not decided yet.
2. **Repository name and visibility** (public or private). Not decided yet.

---

## 11. References

"Opened" means I read the page while preparing this document. "Listed" means the link appears on a page I read, but I did not open it. "Not opened" means it comes from my own knowledge and should be checked before you rely on it.

| Topic | Source | Status |
|---|---|---|
| Original rough plan (superseded by this document) | Project doc `chess engine idea.md` | Read |
| Magic-bitboard article | Uploaded text file (Wikipedia-style article on magic bitboards, Kannan's method) | Read |
| C++23 features and compiler versions | cppreference, [C++23](https://en.cppreference.com/w/cpp/23) | Opened (first 100,000 of 114,434 characters) |
| Compiler support table | cppreference, [C++23 compiler support](https://en.cppreference.com/w/cpp/compiler_support/23) | Opened |
| C++26 status | Wikipedia, [C++26](https://en.wikipedia.org/wiki/C%2B%2B26) | Opened |
| `CXX_STANDARD` values | CMake docs, [CXX_STANDARD](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html) | Opened |
| Test framework | [Catch2](https://github.com/catchorg/Catch2) | Opened. Tag `v3.16.1` checked with `git ls-remote` |
| Match runner | [Cute Chess / cutechess-cli](https://github.com/cutechess/cutechess) | Opened. Tag `v1.5.1` checked with `git ls-remote`. The page's example uses xboard, so UCI use is from my knowledge |
| Reference engine | [Stockfish](https://github.com/official-stockfish/Stockfish) and its [UCI wiki page](https://github.com/official-stockfish/Stockfish/wiki/UCI-&-Commands) | Main page opened. Wiki page failed to load |
| UCI background | Chess Programming Wiki, [UCI](https://www.chessprogramming.org/UCI) | Opened |
| UCI specification | [Shredder Chess](https://www.shredderchess.com/chess-features/uci-universal-chess-interface.html), [WBEC Ridderkerk](http://wbec-ridderkerk.nl/html/UCIProtocol.html) | Listed |
| Magic bitboards | Chess Programming Wiki, [Magic Bitboards](https://www.chessprogramming.org/Magic_Bitboards) | Opened |
| Perft values | Chess Programming Wiki, [Perft Results](https://www.chessprogramming.org/Perft_Results) | Opened |
| Starting values for evaluation tables | Chess Programming Wiki, [Simplified Evaluation Function](https://www.chessprogramming.org/Simplified_Evaluation_Function) | Not opened. The piece values in section 7.4 match what I remember of it |

---

## 12. Change log

| Date | Change |
|---|---|
| 2026-10-09 | Draft 1, written by Claude |
| 2026-10-10 | Your edits reviewed and merged. Scope, platform (Ubuntu), project name (ICE) and GitHub added. Plan content (search, evaluation) moved into the document so the plan can be retired. Future items moved to ICE-FUTURE-IMPROVEMENTS.md. Table and cross-reference errors fixed. Final. |
