// Attack tables and attack functions.
//
// Knights, kings and pawns always attack the same squares from a given square, so
// their attacks are lookup tables built at compile time (spec decision D5).
//
// Rooks, bishops and queens depend on which squares are blocked. In v0 they use plain
// "ray loops": walk in each direction until a blocker is hit (decision D6). The loops
// are slow but obviously correct, and they stay in the code base as the reference
// that faster methods (magic bitboards) are later checked against.
#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "bitboard.h"
#include "types.h"

namespace ice {

namespace detail {

// One (file delta, rank delta) pair per step a leaper can take.
template <std::size_t N>
using Steps = std::array<std::array<int, 2>, N>;

inline constexpr Steps<8> kKnightSteps{
    {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
inline constexpr Steps<8> kKingSteps{
    {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};

// For every square, the set of squares reachable with one of the given steps while
// staying on the board.
template <std::size_t N>
constexpr std::array<Bitboard, kSquareCount> make_leaper_table(const Steps<N>& steps) {
  std::array<Bitboard, kSquareCount> table{};
  for (int s = 0; s < kSquareCount; ++s) {
    Bitboard attacks = 0;
    for (const auto& [df, dr] : steps) {
      const int f = (s & 7) + df;
      const int r = (s >> 3) + dr;
      if (f >= 0 && f < 8 && r >= 0 && r < 8) attacks |= Bitboard{1} << (r * 8 + f);
    }
    table[static_cast<std::size_t>(s)] = attacks;
  }
  return table;
}

// Squares a pawn of each color attacks from each square (diagonally forward).
constexpr std::array<std::array<Bitboard, kSquareCount>, kColorCount> make_pawn_table() {
  std::array<std::array<Bitboard, kSquareCount>, kColorCount> table{};
  for (int s = 0; s < kSquareCount; ++s) {
    const int f = s & 7;
    const int r = s >> 3;
    Bitboard white = 0;
    Bitboard black = 0;
    for (const int df : {-1, 1}) {
      const int nf = f + df;
      if (nf < 0 || nf > 7) continue;
      if (r < 7) white |= Bitboard{1} << ((r + 1) * 8 + nf);
      if (r > 0) black |= Bitboard{1} << ((r - 1) * 8 + nf);
    }
    table[idx(Color::White)][static_cast<std::size_t>(s)] = white;
    table[idx(Color::Black)][static_cast<std::size_t>(s)] = black;
  }
  return table;
}

}  // namespace detail

inline constexpr auto kKnightAttacks = detail::make_leaper_table(detail::kKnightSteps);
inline constexpr auto kKingAttacks = detail::make_leaper_table(detail::kKingSteps);
inline constexpr auto kPawnAttacks = detail::make_pawn_table();

constexpr Bitboard knight_attacks(Square s) noexcept { return kKnightAttacks[idx(s)]; }
constexpr Bitboard king_attacks(Square s) noexcept { return kKingAttacks[idx(s)]; }
// The squares a pawn of color `c` standing on `s` attacks.
constexpr Bitboard pawn_attacks(Color c, Square s) noexcept { return kPawnAttacks[idx(c)][idx(s)]; }

// Sliding attacks given the set of occupied squares `occ` (both colors). The result
// includes the first blocker on each ray (it can be captured if it is an enemy) and
// the caller removes friendly pieces.
Bitboard rook_attacks(Square s, Bitboard occ) noexcept;
Bitboard bishop_attacks(Square s, Bitboard occ) noexcept;
inline Bitboard queen_attacks(Square s, Bitboard occ) noexcept {
  return rook_attacks(s, occ) | bishop_attacks(s, occ);
}

// Attacks of a non-pawn piece type. Pawns are special (direction and pushes), so
// they are not accepted here.
inline Bitboard piece_attacks(PieceType t, Square s, Bitboard occ) noexcept {
  switch (t) {
    case PieceType::Knight: return knight_attacks(s);
    case PieceType::Bishop: return bishop_attacks(s, occ);
    case PieceType::Rook: return rook_attacks(s, occ);
    case PieceType::Queen: return queen_attacks(s, occ);
    case PieceType::King: return king_attacks(s);
    case PieceType::Pawn: break;
  }
  std::unreachable();  // callers must not ask for pawn attacks here
}

}  // namespace ice
