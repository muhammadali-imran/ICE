# ICE Architecture

## Scope

This document describes the code that exists today. ICE is a C++23 chess rules
and move-generation core with command-line inspection and perft tools. UCI,
search, and evaluation are planned work, not part of the current architecture.

## Runtime flow

```text
src/main.cpp
    ├── FEN / board inspection ──> ice::Board
    └── perft / divide ─────────> ice::perft*()
                                      └── ice::generate_legal()
                                           ├── pseudo-legal move generation
                                           ├── Board::make_move()
                                           ├── Board::in_check()
                                           └── Board::unmake_move()
```

`ice_core` contains the reusable chess primitives, board state, move
generation, and perft. `ice` is a thin command-line executable over that
library. `ice_tests` exercises the core with Catch2.

## Position representation

`ice::Board` keeps piece locations in bitboards indexed by color and piece
type. It also maintains per-color and total occupancy bitboards, plus a
64-square mailbox for direct piece lookup. Mutations update these
representations together. Side to move, castling rights, en-passant target,
clocks, Zobrist key, and a fixed-size undo history are held with the position.

Squares use little-endian rank-file numbering: `a1 == 0`, files increase
within a rank, and `h8 == 63`. A bitboard is a 64-bit unsigned integer with
bit `n` representing square `n`.

## Attacks and moves

Knight, king, and pawn attacks are precomputed at compile time. Rook and
bishop attacks use directional ray walks that include the first occupied
square; queen attacks combine those results.

Move generation first builds pseudo-legal moves, then tests each candidate by
making it, checking whether the mover's king is attacked, and unmaking it.
Fixed-capacity move and undo arrays avoid heap allocation in these paths.
Moves are packed into 16 bits with source, destination, and flags.

## State and correctness invariants

- All redundant board representations must describe the same position.
- `Board::key()` must equal `Board::compute_key()` for the current position.
- A make/unmake pair restores the full position, including metadata and hash.
- Legal move generation must not leave the moving side's king in check.
- Perft counts for the published reference positions are the main integration
  check for move-generation correctness.

## Module dependencies

- `types.h` defines colors, pieces, squares, and castling-right constants.
- `bitboard.h`, `move.h`, and `zobrist.h` provide value-level primitives.
- `attacks.h` / `attacks.cpp` provide attack queries.
- `board.h` / `board.cpp` own position state, FEN, hashing, and make/unmake.
- `movegen.h` / `movegen.cpp` generate pseudo-legal and legal moves.
- `perft.h` / `perft.cpp` count legal move trees and print divide output.
- `main.cpp` parses command-line commands and writes user-facing output.

See [STRUCTURE.md](STRUCTURE.md) for the repository map and [SPEC.md](SPEC.md)
for the current behavior and project boundaries. The planned UCI, search, and
evaluation design is preserved separately in [ENGINE-DESIGN.md](ENGINE-DESIGN.md).
