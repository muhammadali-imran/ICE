# Test Cases

## Running tests

Configure, build, and run the standard suite:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

The Debug configuration enables AddressSanitizer and UBSan by default. The
Release CTest configuration also registers the depth-5 perft suite:

```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

Catch2 tags can be run directly, for example `build/release/ice_tests
"[slow]"` for depth 5 or `build/release/ice_tests "[.deep]"` for depth 6.
The latter is hidden from normal runs and is not registered as a regular
CTest test.

## Implemented coverage

| Area | Cases covered |
|---|---|
| Types and moves | Square/name conversion; packed source, destination, and flags; capture, promotion, castling, and en-passant flag queries; null move; UCI long algebraic formatting |
| Bitboards | Population counts; least-significant-bit iteration; multiple-bit detection; color-relative forward shifts; file and rank masks |
| Attacks | Knight, king, and pawn edge behavior; rook and bishop blocker handling; queen attack composition; ray attacks checked against an independent implementation across deterministic random occupancies |
| FEN and board state | Start position; exact round-trips; optional counters; malformed/impossible FEN rejection without changing the board; board diagram; check detection |
| Make/unmake and hashing | Zobrist behavior across transpositions, side-to-move and en-passant state; make/unmake restoration and incremental-vs-recomputed keys |
| Special moves and legality | En-passant capture and restoration; double-push target; castling rook/right updates and attacked/blocked castling paths; rook move/capture rights; promotion and restoration; pinned moves; en passant exposing the king; no legal moves in checkmate/stalemate positions |
| Perft | Depth 0; depth 1–4 in the standard suite; depth 5 in slow tests; depth 6 in the hidden deep test; divide total and known root-move count |

## Perft references

Counts below are checked by the suite wherever a nonzero value is present in
the test data. Routine tests cover depths 1–4; depth 5 is slow and depth 6 is
hidden by default.

| Position | d1 | d2 | d3 | d4 | d5 | d6 |
|---|---:|---:|---:|---:|---:|---:|
| Start | 20 | 400 | 8,902 | 197,281 | 4,865,609 | 119,060,324 |
| Kiwipete | 48 | 2,039 | 97,862 | 4,085,603 | 193,690,690 | 8,031,647,685 |
| Position 3 | 14 | 191 | 2,812 | 43,238 | 674,624 | 11,030,083 |
| Position 4 | 6 | 264 | 9,467 | 422,333 | 15,833,292 | 706,045,033 |
| Position 4 mirrored | 6 | 264 | 9,467 | 422,333 | 15,833,292 | 706,045,033 |
| Position 5 | 44 | 1,486 | 62,379 | 2,103,487 | 89,941,194 | — |
| Position 6 | 46 | 2,079 | 89,890 | 3,894,594 | 164,075,551 | 6,923,051,137 |

`perft_test.cpp` contains the corresponding FEN strings. Each perft case also
checks that the board FEN and Zobrist key are restored after traversal.

The reference positions, including the mirrored Position 4, are:

| Position | FEN |
|---|---|
| Start | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` |
| Kiwipete | `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1` |
| Position 3 | `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1` |
| Position 4 | `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1` |
| Position 4 mirrored | `r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1` |
| Position 5 | `rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8` |
| Position 6 | `r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10` |

The original source lists Kiwipete without move counters; `0 1` is appended.
Depth 6 and beyond can be expensive. The full v0 acceptance target includes
depths 1–5; the current standard test run covers depths 1–4, depth 5 is slow,
and depth 6 is a dedicated final check. The original acceptance reference
lists depth-6 values for Kiwipete and Position 6, but the current Catch2 test
data omits those counts (zero means not checked); the table records reference
targets, not a claim that every value is currently tested.

## Complete-engine acceptance targets

These checks preserve the original v0 acceptance plan. They are not all
currently runnable because UCI, search, evaluation, and draw adjudication are
not implemented.

| Check | Requirement | Proposed method | Pass condition |
|---|---|---|---|
| FEN round-trip | FR1 | Parse and print all perft FENs plus positions with no castling rights, en-passant state, and move counters. | Supported input round-trips exactly, subject to documented normalization. |
| Zobrist consistency | FR3 | Compare incremental and recomputed keys after make/unmake and perft traversal. | Keys always match. |
| Draw rules | FR4 | Script stalemate, fifty-move, repetition, and insufficient-material positions (K vs K, K+B vs K, K+N vs K). | Each intended draw is recognized according to documented rules. |
| Determinism | NFR2 | Run the same fixed-depth position twice with one thread. | Same best move and node count. |
| Sanitizers | NFR3 | Run the full suite and perft in Debug with sanitizers. | No sanitizer reports. |
| UCI session | FR7 | Script `uci`, `isready`, `position`, and `go depth`; also load in Cute Chess. | Expected protocol responses and GUI play. |
| Tactics | G3 | Test mate-in-one through mate-in-three and win-a-piece positions at fixed limits. | All positions are solved according to the suite. |
| Reliability | G2, NFR7 | At least 100 self-play and limited-Stockfish games at rapid time control. | No illegal moves, crashes, hangs, or time forfeits. |
| Calibration | G4 | 100+ games against Stockfish near strength settings 1500 and 1800–2000. | Clear majority of wins under documented match conditions. |
| Human check | G5 | Play rapid games at the intended human level. | Engine holds its own at that level. |

For strength matches, use the controlled protocol in [ROADMAP.md](ROADMAP.md):
single-threaded engines, matched hash/time controls, varied openings with each
position played from both sides, and at least 200 games or a pre-declared SPRT
decision unless the specific acceptance target states otherwise.

## Not yet covered / future acceptance work

There are no tests yet for UCI sessions, search/evaluation, draw adjudication,
engine-vs-engine strength, or sustained game reliability because those
features are not implemented. Add focused tests as each feature is introduced;
the complete acceptance targets are listed above and planned implementation
work is tracked in [ROADMAP.md](ROADMAP.md).
