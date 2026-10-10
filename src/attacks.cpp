#include "attacks.h"

namespace ice {

namespace {

// Walks from `s` in each of the given directions until it leaves the board or hits an
// occupied square. The blocking square itself is included in the result.
template <std::size_t N>
Bitboard slide(Square s, Bitboard occ, const std::array<std::array<int, 2>, N>& directions) noexcept {
  Bitboard attacks = 0;
  for (const auto& [df, dr] : directions) {
    int f = file_of(s) + df;
    int r = rank_of(s) + dr;
    while (f >= 0 && f < 8 && r >= 0 && r < 8) {
      const Bitboard square = Bitboard{1} << (r * 8 + f);
      attacks |= square;
      if ((occ & square) != 0) break;  // blocked: nothing beyond this square is attacked
      f += df;
      r += dr;
    }
  }
  return attacks;
}

constexpr std::array<std::array<int, 2>, 4> kRookDirections{{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
constexpr std::array<std::array<int, 2>, 4> kBishopDirections{{{1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

}  // namespace

Bitboard rook_attacks(Square s, Bitboard occ) noexcept { return slide(s, occ, kRookDirections); }
Bitboard bishop_attacks(Square s, Bitboard occ) noexcept { return slide(s, occ, kBishopDirections); }

}  // namespace ice
