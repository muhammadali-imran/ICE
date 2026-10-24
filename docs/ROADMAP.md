# ICE: Roadmap and Future Improvements

| | |
|---|---|
| **Document** | ROADMAP.md |
| **Project** | ICE |
| **Status** | Final |
| **Date** | 2026-10-10 |
| **Companions** | [SPEC.md](SPEC.md) |

This document records planned milestones and possible future improvements. Items here are proposals, not current implementation guarantees. See [SPEC.md](SPEC.md) for the current project scope and implemented behavior.

---

## 1. Rules for making improvements

- **One change at a time.** Each change goes into its own commit or branch on GitHub.
- **Test every change that can affect strength head-to-head.** The new version plays the previous version in cutechess-cli. Keep it only if it wins. An SPRT (sequential probability ratio test, a statistical stopping rule) saves games compared with a fixed-length match. As far as I know cutechess-cli supports it, but I could not confirm that or the exact flags, so check `cutechess-cli -help`.
- **Speed changes** (no strength intent) must keep perft identical and the `bench` node count identical at fixed depth, then be timed.
- **Record the result** in the release notes and in the status of the item below.
- **Measure before you optimize.** Profile first, then change.
- **Effects below are qualitative** (small, medium, large) and come from general engine-programming experience. I have not measured any of them for ICE. The real numbers come from your own matches.

Effort: **S** (an evening), **M** (a few days), **L** (a week or more).

### 1.1 Standard match protocol (used by the acceptance tests below)

Unless an acceptance test says otherwise:

- Both engines single-threaded, same hash size, same time control (a rapid control for human-like play or a fast control such as 10+0.1 for quick comparisons).
- Games start from a varied set of opening positions, with each position played once from each side.
- At least 200 games, or an SPRT that reaches a decision.
- Pass means the new version is statistically ahead, and there are zero illegal moves, crashes or time forfeits.

---

## 2. Milestones on the way to ICE v0

These are the original build steps toward the first complete engine release. The exit tests are tracked in [TEST_CASES.md](TEST_CASES.md) and the relevant release plan below.

| Version | Content | Exit test |
|---|---|---|
| **v0.1** | Types, bitboard helpers, FEN parse and print | FEN round-trips on a set of positions |
| **v0.2** | Knight, king, pawn tables, ray-loop sliders, pseudo-legal movegen, make/unmake, legality filter | **Perft matches [TEST_CASES.md](TEST_CASES.md)** at d1 to d5 for all six positions, and the mirrored Position 4 |
| **v0.3** | UCI loop (`uci`, `isready`, `position`, `go depth`, `bestmove`), `perft` command | Cute Chess can load ICE and play it (first legal move is enough) |
| **v0.4** | Evaluation (material, PST), negamax alpha-beta, quiescence, MVV-LVA, iterative deepening, time control, draw detection | Plays full games with no illegal moves or time losses. Finds mate in 1 to 3 on test positions |
| **v0.5** | Zobrist hashing, transposition table, killers, history | Fewer nodes at the same depth than v0.4. Zobrist key after make/unmake equals the key recomputed from scratch |
| **v0.6** | Fuller evaluation (pawn structure, king safety, mobility, bishop pair, tapering) | Wins the head-to-head match against v0.5 |
| **v0.7** | Calibration against Stockfish and a human game | The intended end-to-end acceptance checks pass. This is **ICE v0** |

---

## 3. Milestones after ICE v0

Each milestone is a release tagged on GitHub. The "Pass" column is the acceptance test, and every milestone also needs the perft values in [TEST_CASES.md](TEST_CASES.md) and a sanitizer run to still pass.

| Release | Theme | Items | Pass |
|---|---|---|---|
| **ICE v1.0** | Solid and fast | E1, E2, E3, E4, E10, A1, A4, B9, B1, B6, B7, B5, B8 | Perft identical to v0. `bench` node count stable between builds. Nodes per second higher than ICE v0 on the same machine (recorded). Wins the standard match against ICE v0 |
| **ICE v1.1** | Deeper search | A6, A2, A3, A7, A5, A8, A9, A11, A10 | Wins the standard match against v1.0. Tactical suite: solved count not lower than v1.0. No time forfeits in 100 games at a rapid control |
| **ICE v1.2** | Smarter evaluation | C1, C3, C4, C2, then C5 | Wins the standard match against v1.1. The tuned evaluation (C5) beats the hand-set one in a match |
| **ICE v1.3** | Useful to play with | D1, D4, D7, E6, F8 | A lower strength setting loses to a higher one in a match, at several levels. Games from the book are varied (no repeated game in a 50-game match). Analysis output matches the evaluation terms |
| **ICE v2.0** | Stretch | D2, D3, D5, D6, B2, B3, B4, F1 to F7, C6, E5, E7, E8, E9 | Per item: threads beat one thread at equal wall-clock time; tablebase probes agree with known endgame results; pondering gains time without losing on the clock; each remaining item has its own test in its table below |

Suggested order inside a release follows the order of the items listed.

---

## 4. Tier A: Strength from search

| # | Improvement | What it does | Effect | Effort | Depends on | Pass test |
|---|---|---|---|---|---|---|
| A1 | **Principal variation search (PVS)** | Searches the first move with a full window and the rest with a null window, re-searching only if needed | Medium | S | Good move ordering | Same best move at fixed depth, fewer nodes |
| A2 | **Null-move pruning** | Gives the opponent a free move. If the position still fails high, prune. Skipped in check and in zugzwang-prone endgames | Large | S to M | TT, check detection | Wins the standard match. Mate and zugzwang test positions still solved |
| A3 | **Late move reductions (LMR)** | Searches late, unpromising quiet moves to a shallower depth first | Large | M | Good move ordering, PVS | Wins the standard match. Tactics suite shows no regressions |
| A4 | **Check extensions** | Searches one ply deeper when in check | Small to medium | S | | Wins the standard match |
| A5 | **Aspiration windows** | Starts each iteration with a narrow window around the previous score | Small | S | Iterative deepening | Fewer nodes at equal depth |
| A6 | **Static exchange evaluation (SEE)** | Estimates the net material result of a capture sequence on one square without searching it | Medium | M | Attack boards | Better capture ordering. Clearly losing captures pruned in quiescence |
| A7 | **Futility and reverse futility pruning, delta pruning** | Skips moves that cannot raise alpha given the static eval | Medium | M | Reliable eval | Wins the standard match |
| A8 | **Countermove heuristic, butterfly-table history** | Orders quiet moves using what refuted the previous move | Small to medium | S | History tables | Wins the standard match |
| A9 | **Internal iterative deepening or reductions when there is no TT move** | Finds a move to try first | Small | S | TT | Wins the standard match |
| A10 | **Better time management** | Spends more time on unstable or critical positions, less on forced moves | Medium in games | M | | Fewer time losses. Wins the standard match at the same clock |
| A11 | **Checks in quiescence search** | Extends quiescence to include checking moves near the horizon, not just captures and promotions | Small to medium | M | Quiescence search | Wins the standard match. Node count stays reasonable |

## 5. Tier B: Speed

| # | Improvement | What it does | Effect | Effort | Pass test |
|---|---|---|---|---|---|
| B1 | **Magic bitboards for rooks, bishops, queens** | Replaces the current ray loops with one multiply-shift lookup | Large for nodes per second | M | Perft identical. Every table entry equals the ray-loop result for every blocker subset |
| B2 | **Fancy magics with per-square table sizes**, or Black Magic | Smaller tables, better cache behaviour | Small | M | Perft identical. Compare nodes per second on your CPU |
| B3 | **PEXT/BMI2 indexing** (hardware bit extract) | Replaces the multiply with one instruction where the CPU has BMI2 | Small to medium on supported CPUs | S | Switch at build time, perft identical. Note the portability cost |
| B4 | **Pin and check masks, legal-by-construction movegen** | Removes make/unmake just to test legality | Medium | L | Perft identical. Compare against the filter version on all positions |
| B5 | **Staged move generation** | Generates the hash move first, then captures, then killers, then quiets only if needed | Medium | M | Same results, fewer nodes generated |
| B6 | **Incremental evaluation** (update PST and material during make/unmake) | Avoids recomputing the whole eval | Medium | M | Incremental value equals full recompute at every node in a debug build |
| B7 | **Transposition-table tuning** (bucketed entries, replacement scheme, prefetch) | Better hit rate and fewer cache misses | Medium | M | Wins the standard match at equal time |
| B8 | **Compiler optimization** (LTO, profile-guided optimization, `-march` builds) | Free speed from the toolchain | Small to medium | S | Same `bench` node count, higher nodes per second |
| B9 | **Bulk counting in perft** | Counts leaf moves without making them. Speeds up testing, not the engine | Testing only | S | Same perft values |

## 6. Tier C: Evaluation

| # | Improvement | What it does | Effect | Effort | Pass test |
|---|---|---|---|---|---|
| C1 | **Refined tapering** with tuned game-phase weights | Smoother transition between middlegame and endgame | Small to medium | S | Wins the standard match |
| C2 | **Pawn hash table** | Caches pawn-structure scores, which change slowly | Small (speed) | S | Same eval with and without |
| C3 | **Passed-pawn, king-safety and mobility refinement** | Richer terms, using attack-board popcounts | Medium | M | Wins the standard match |
| C4 | **Endgame knowledge**: king and pawn vs king, drawn material combinations, mop-up for won endings | Stops the engine failing to convert or fleeing from draws | Medium in endgames | M | Test positions: converts known wins, recognizes known draws |
| C5 | **Evaluation tuning**: Texel's tuning method, or SPSA self-play tuning | Fits weights to game results instead of guessing them | Large relative to hand tuning | L | Wins the standard match. The tuner stays in `tools/` |
| C6 | **NNUE-style neural evaluation** | A small efficiently updatable network replaces the hand-written eval | Very large, but a different project | L+ | Revisit only after C5. Pass: wins the standard match against the best hand-written version |

## 7. Tier D: Features

| # | Improvement | What it does | Effort | Notes |
|---|---|---|---|---|
| D1 | **More UCI options**: `Threads`, `MultiPV`, `UCI_LimitStrength` and `UCI_Elo` (so *ICE* can be set to play at a chosen strength), `Ponder` | Makes ICE a better sparring partner for human play | S to M | A strength-limit option is useful for you as a learner |
| D2 | **Multithreading (Lazy SMP)** | Several threads search the same tree and share the transposition table | L | Needs a thread-safe TT and breaks determinism (NFR2). Keep a single-thread mode for testing |
| D3 | **Pondering** | Thinks on the opponent's time | M | Needs correct handling of `ponderhit` and `stop` |
| D4 | **Opening book** (for example the Polyglot format) | Varied, sound openings without calculating them | M | Add some randomness so games do not repeat |
| D5 | **Endgame tablebases (Syzygy)** | Perfect play in positions with few pieces | L | Requires probing code or a library and downloaded table files. License and size need checking |
| D6 | **Chess960 support** | Different start positions and castling rules | M | Touches FEN, castling and UCI |
| D7 | **Analysis mode and explanation output** | Prints evaluation terms for a position | M | Fits your learning goal |

## 8. Tier E: Testing and tooling

| # | Improvement | Purpose | Effort |
|---|---|---|---|
| E1 | **Continuous integration on GitHub** (GitHub Actions) building with GCC and Clang, debug (sanitizers) and release, running perft and unit tests on each push | Catch regressions early | S to M |
| E2 | **Tactical test suites** (mate-in-N sets and "win a piece" positions) run by a script | Quick strength check between matches | S |
| E3 | **`bench` command** with a fixed set of positions and a fixed depth, printing total nodes and nodes per second | Detects accidental behaviour changes in speed refactors | S |
| E4 | **Match scripts and result logging** in `tools/` | One command to compare any two builds | S |
| E5 | **Fuzzing the FEN and UCI parsers** | Robustness against malformed input | M |
| E6 | **Strength ladder**: record each tagged release's results against Stockfish at several strength settings | Tracks progress toward and beyond the 1500 to 2000 target | S |
| E7 | **Windows and macOS builds** (including MSVC) | Wider platform support | M |
| E8 | **Formatting and linting** (`clang-format`, `clang-tidy`) | Consistent style, extra bug checks | S |
| E9 | **Profiling** (`perf`, `valgrind --tool=callgrind`) | Find real hot spots before speed work | S |
| E10 | **Release process on GitHub**: tagged releases with a Linux binary and release notes (perft and match results) | Anyone can run a specific version | S |

## 9. Tier F: Platform, language and integration

| # | Item | Details | Effort |
|---|---|---|---|
| F1 | **Raise the compiler floor** to GCC 14 or newer | Unlocks deducing `this`, `std::print`, `std::generator`, `std::stacktrace` (tracker). Only if a feature actually helps | S |
| F2 | **`std::expected` for errors** across FEN and UCI parsing | Cleaner error paths without exceptions. First find out why it failed on Clang 18 (tracker) | S |
| F3 | **`import std;`** | Faster builds once CMake and the compiler both support it well | M |
| F4 | **Evaluate C++26** | C++26 was finalized in March 2026 and GCC 16.1 supports most of it. Candidates: `std::simd` for evaluation, `inplace_vector` for move lists, hardened standard library for debug builds. Adopt only if measured to help | M |
| F5 | **Web build via WebAssembly (Emscripten)** | Runs ICE in a browser | M to L |
| F6 | **Integration with other projects** (for example BOW) | Options: the other project launches ICE as a UCI subprocess; or ICE becomes a library; or a WASM build runs in the page | M |
| F7 | **Python tooling around ICE** | Analysis scripts, match statistics, and (if C6 happens) data generation for training. Uses your existing Python skills | M |
| F8 | **ICE as a library** with a clean API (`Board`, `search`, `evaluate`) separate from UCI | Reuse in other projects and in tests | M |

---

## 10. Review checkpoints

At the end of each release:

1. Update the strength ladder (E6) with the new release's results.
2. Update [SPEC.md](SPEC.md) and [ARCHITECTURE.md](ARCHITECTURE.md) if a decision, requirement, or implementation changes.
3. Decide whether the 1500 to 2000 target is met using the match results and human play checks. If it is, later releases become optional.

## 11. Change log

| Date | Change |
|---|---|
| 2026-10-09 | Draft 1: tiers A to F, gate, rules, roadmap |
| 2026-10-10 | Milestones (v0.1 to v0.7) moved here from the spec. Post-v0 milestones with acceptance tests added. Items A11, E8, E9, E10 added. Windows and macOS, linting and profiling moved here from the spec. Final. |
