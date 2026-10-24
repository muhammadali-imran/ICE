# Engine Design: Original ICE v0 Target

This document preserves the detailed protocol, search, and evaluation design
from the original ICE v0 specification. These sections define a **target
design**, not features currently available in the executable. Current
implementation architecture is described in [ARCHITECTURE.md](ARCHITECTURE.md).

## UCI protocol target

UCI (Universal Chess Interface) is a text protocol over standard input and
output. The GUI supplies the full position and clock context; it acts as game
arbiter. The following commands and responses were the original ICE target and
must be checked against a current UCI reference during implementation.

| Direction | Command or response | Purpose |
|---|---|---|
| GUI → engine | `uci` | Identify the engine, report options, finish with `uciok`. |
| GUI → engine | `isready` | Reply `readyok` when ready. |
| GUI → engine | `ucinewgame` | Clear hash and search history. |
| GUI → engine | `position startpos moves ...` or `position fen <fen> moves ...` | Set the position and optionally apply moves. |
| GUI → engine | `go wtime btime winc binc movestogo` | Search with clock values. |
| GUI → engine | `go depth N`, `movetime T`, `nodes N`, or `infinite` | Run fixed-limit or unbounded search. |
| GUI → engine | `stop`, `quit` | Stop search or exit. |
| GUI → engine | `setoption name Hash value N` | Set transposition-table size in MB. |
| Engine → GUI | `info depth ... seldepth ... score cp ... nodes ... nps ... time ... pv ...` | Report search progress. |
| Engine → GUI | `bestmove e2e4` | Return the selected move. |
| Debug extensions | `perft N`, `d` | Perft and board inspection, not standard UCI commands. |

The original implementation note suggested parsing with
`std::string_view` and `std::istringstream`; parsing must still correctly
handle the protocol's token and position grammar.

## Bitboards

A bitboard is a `std::uint64_t` where bit `i` represents square `i`. Typical
operations include setting, clearing, and testing a square, population counts
with `std::popcount`, finding the least-significant bit with
`std::countr_zero`, and clearing it with `bb &= bb - 1`.

For a bitboard of pawns, file masks prevent attack shifts from wrapping across
the board edge:

```cpp
constexpr std::uint64_t kNotA = 0xfefefefefefefefeULL;
constexpr std::uint64_t kNotH = 0x7f7f7f7f7f7f7f7fULL;
auto white_pawn_attacks = ((pawns << 7) & kNotH) | ((pawns << 9) & kNotA);
auto black_pawn_attacks = ((pawns >> 7) & kNotA) | ((pawns >> 9) & kNotH);
```

Pawn pushes advance one rank into unoccupied squares. Double pushes are
restricted to the pawn's starting rank and require both traversed squares to
be clear. Every shift and square index must be guarded against invalid
amounts.

## Sliding attacks and magic-bitboard proposal

The current implementation uses ray walks for rook and bishop attacks; queen
attacks combine both. The ray implementation includes the first blocker and
serves as a clear reference.

The proposed magic-bitboard lookup for each square:

1. Mask relevant occupancy along the slider rays, excluding board-edge
   squares whose occupancy does not alter the attack result.
2. Multiply the masked occupancy by a square-specific 64-bit magic number.
3. Shift right by `64 - N`, where `N` is the number of relevant occupancy
   bits.
4. Use the result to index a precomputed attack table.

```cpp
index   = ((occupancy & mask[sq]) * magic[sq]) >> (64 - bits[sq]);
attacks = table[sq][index];
```

This multiplication-and-shift is a perfect-hash-style index. Different
blocker sets that produce different attack sets must not collide; blocker sets
with identical attacks may share an index (a constructive collision).
Candidate magic numbers are commonly found by testing sparse random values
against every relevant blocker subset. The ray implementation supplies the
reference attacks. The carry-rippler expression for enumerating subsets is
`subset = (subset - mask) & mask`.

| Variant | Original proposal | Approximate size / requirement |
|---|---|---|
| Plain magic | Fixed-size table per square. | About 2 MiB for rook tables; bishop tables add memory. |
| Fancy magic | Per-square table sizes and shifts. | About 800 KiB for combined rook and bishop tables. |
| Black magic | OR complement of the mask rather than ANDing the occupancy mask. | About 692 KiB in the original cited estimates. |
| PEXT/BMI2 | Hardware bit extraction for indexing. | Requires a CPU with BMI2 and a portability decision. |

Memory estimates are approximate and depend on table layout. Any replacement
must be checked against the ray implementation for every relevant occupancy
subset and must preserve perft.

## Search target

The planned iterative-deepening flow is:

```text
Iterative deepening (depth 1..N)
        |
        v
Move ordering (hash/PV, MVV-LVA, killers, history)
        |
        v
Alpha-beta search with transposition table
        | at depth zero
        v
Quiescence search -> static evaluation
```

### Negamax alpha-beta

Negamax expresses minimax from the side-to-move perspective, allowing one
recursive search function. Alpha is the best score already guaranteed to the
side to move; beta is the bound the opponent will not allow. When alpha is at
least beta, remaining moves cannot affect the result and are pruned.
Well-ordered alpha-beta can substantially reduce the search tree.

### Iterative deepening and time

Search depths 1 through N in sequence, checking the time budget. Return the
best move from the last completed iteration if time expires. Use the previous
iteration's best move first on the next iteration. Allocate a fraction of the
remaining clock and increment per move, poll time periodically, and retain a
safety margin to prevent forfeits.

### Move ordering

The original ordering proposal is:

1. Transposition-table hash move / principal-variation move.
2. Captures ordered by MVV-LVA (most valuable victim, least valuable attacker);
   include promotions.
3. Killer quiet moves that caused beta cutoffs at the same depth in sibling
   branches.
4. Remaining quiet moves by history scores.

### Quiescence search

At the nominal depth limit, continue searching captures and promotions until
the position is quiet to reduce horizon-effect errors during exchanges. The
original design allows a stand-pat static score. Exact handling of check,
terminal positions, and pruning must be specified and tested when implemented.

### Transposition table and Zobrist hashing

The target table is keyed by a 64-bit Zobrist key formed from random values
for piece-square occupancy, castling rights, en-passant file, and side to
move. Keys are updated incrementally during make/unmake. Entries store a key
or key fragment, score, searched depth, best move, and bound type (exact,
lower, or upper). Mate scores are stored relative to the node and adjusted by
ply when retrieved. The intended UCI `Hash` option controls size, with a
16 MB default.

The current Board already computes and incrementally maintains a Zobrist key,
but it has no search transposition table.

## Evaluation target

Evaluation scores are centipawns (100 equals one pawn) from the side-to-move
perspective. The original hand-written function was:

```text
Eval = material + position + king safety + pawn structure + mobility
```

### Material values

| Piece | Centipawns | Notes |
|---|---:|---|
| Pawn | 100 | |
| Knight | 320 | |
| Bishop | 330 | Original proposal adds a 50-centipawn bishop-pair bonus. |
| Rook | 500 | |
| Queen | 900 | |
| King | 20,000 | Sentinel-scale value; the king is never captured. |

### Positional terms

- Piece-square tables: one 64-square table per piece type and game phase.
- Knights are rewarded near the center and discouraged on the rim.
- Pawns are rewarded for advancement and central control.
- Rooks receive open- and semi-open-file bonuses.
- Middlegame kings favor castled safety; endgame kings favor centralization.
- Taper middlegame/endgame tables by remaining non-pawn material. Black's
  tables are the vertically flipped White tables.

### King safety

- Penalize weakened or missing pawn shields around castled king positions.
- Penalize enemy attack pressure and proximity in the king zone.
- The original proposal suggested a castling-status term when castling rights
  are lost without castling; this should be validated empirically before use.

### Pawn structure

- Penalize isolated pawns with no friendly pawn on adjacent files.
- Penalize doubled pawns on the same file.
- Reward passed pawns according to rank, when no enemy pawn can stop them on
  the same or adjacent files ahead.

### Mobility

For each piece, count attacked squares not occupied by friendly pieces and
scale by a small piece-specific weight.

Weights and tables were intended to start from simple published examples and
then be adjusted by head-to-head matches. They are not implemented or tuned
yet.

## Draw handling target

The complete-engine target calls for fifty-move-rule, threefold-repetition,
and insufficient-material detection, in game play and search. The original
insufficient-material examples were king versus king, king and bishop versus
king, and king and knight versus king. Repetition history must be available to
search so it can avoid misjudging drawn lines.

Draw rules and their exact claim/automatic-draw semantics must be validated
against the selected UCI/game behavior before implementation.
