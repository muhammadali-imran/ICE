// Bitboard helpers.
//
// A bitboard is a std::uint64_t in which bit i says "something is true of square i"
// (spec section 7.1). One integer can hold all white pawns, the squares a knight
// attacks, or every occupied square.
#pragma once

#include <bit>
#include <cassert>

#include "types.h"

namespace ice {

inline constexpr Bitboard kFileA = 0x0101010101010101ULL;
inline constexpr Bitboard kFileH = kFileA << 7;
inline constexpr Bitboard kNotFileA = ~kFileA;
inline constexpr Bitboard kNotFileH = ~kFileH;

inline constexpr Bitboard kRank1 = 0xFFULL;
inline constexpr Bitboard kRank2 = kRank1 << 8;
inline constexpr Bitboard kRank3 = kRank1 << 16;
inline constexpr Bitboard kRank4 = kRank1 << 24;
inline constexpr Bitboard kRank5 = kRank1 << 32;
inline constexpr Bitboard kRank6 = kRank1 << 40;
inline constexpr Bitboard kRank7 = kRank1 << 48;
inline constexpr Bitboard kRank8 = kRank1 << 56;

// A bitboard with only square `s` set. Shifting by 64 or more is undefined
// behaviour, hence the assert (Square::None is 64).
constexpr Bitboard bb(Square s) noexcept {
  assert(s != Square::None);
  return Bitboard{1} << sq_num(s);
}

constexpr int popcount(Bitboard b) noexcept { return std::popcount(b); }

// Index of the lowest set bit. Precondition: b != 0.
constexpr Square lsb(Bitboard b) noexcept {
  assert(b != 0);
  return sq_from_num(std::countr_zero(b));
}

// Returns the lowest set square and removes it from `b`.
// This is the standard way to loop over every piece in a bitboard:
//     while (b) { Square s = pop_lsb(b); ... }
constexpr Square pop_lsb(Bitboard& b) noexcept {
  const Square s = lsb(b);
  b &= b - 1;  // clears the lowest set bit
  return s;
}

constexpr bool more_than_one(Bitboard b) noexcept { return (b & (b - 1)) != 0; }

// Moves every bit one rank forward for `c` (up for White, down for Black).
// Bits that fall off the board vanish, so no wrap-around handling is needed.
constexpr Bitboard shift_forward(Bitboard b, Color c) noexcept {
  return c == Color::White ? b << 8 : b >> 8;
}

}  // namespace ice
