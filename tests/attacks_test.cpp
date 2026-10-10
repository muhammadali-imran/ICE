#include <array>
#include <cstdint>
#include <cstdlib>

#include <catch2/catch_test_macros.hpp>

#include "attacks.h"
#include "bitboard.h"

using namespace ice;

namespace {

Bitboard of(std::initializer_list<Square> squares) {
  Bitboard b = 0;
  for (const Square s : squares) b |= bb(s);
  return b;
}

// A second, independently written slider: step by +-1/+-8 (rook) or +-7/+-9 (bishop)
// on the 0..63 numbering and stop when a step wraps around a board edge. Used to
// cross-check the ray loops on random occupancies.
Bitboard reference_slide(Square s, Bitboard occ, const std::array<int, 4>& steps) {
  Bitboard attacks = 0;
  for (const int step : steps) {
    int current = sq_num(s);
    while (true) {
      const int next = current + step;
      if (next < 0 || next > 63) break;
      if (std::abs((next & 7) - (current & 7)) > 1) break;  // wrapped to the other side
      attacks |= Bitboard{1} << next;
      if (((occ >> next) & 1) != 0) break;
      current = next;
    }
  }
  return attacks;
}

}  // namespace

TEST_CASE("bitboard helpers", "[bitboard]") {
  CHECK(popcount(0) == 0);
  CHECK(popcount(kRank1) == 8);
  CHECK(popcount(~Bitboard{0}) == 64);
  CHECK(lsb(bb(Square::E4)) == Square::E4);
  CHECK(lsb(of({Square::C3, Square::H8})) == Square::C3);

  Bitboard b = of({Square::A1, Square::D4, Square::H8});
  CHECK(pop_lsb(b) == Square::A1);
  CHECK(pop_lsb(b) == Square::D4);
  CHECK(pop_lsb(b) == Square::H8);
  CHECK(b == 0);

  CHECK(more_than_one(of({Square::A1, Square::B1})));
  CHECK_FALSE(more_than_one(bb(Square::A1)));
  CHECK_FALSE(more_than_one(0));

  CHECK(shift_forward(bb(Square::E2), Color::White) == bb(Square::E3));
  CHECK(shift_forward(bb(Square::E7), Color::Black) == bb(Square::E6));
  CHECK(shift_forward(bb(Square::E8), Color::White) == 0);  // falls off the board
  CHECK(shift_forward(bb(Square::E1), Color::Black) == 0);
  CHECK(kFileA == of({Square::A1, Square::A2, Square::A3, Square::A4, Square::A5, Square::A6, Square::A7, Square::A8}));
}

TEST_CASE("knight attacks", "[attacks]") {
  CHECK(knight_attacks(Square::A1) == of({Square::B3, Square::C2}));
  CHECK(knight_attacks(Square::H8) == of({Square::G6, Square::F7}));
  CHECK(popcount(knight_attacks(Square::D4)) == 8);
  CHECK(popcount(knight_attacks(Square::B1)) == 3);
  CHECK(knight_attacks(Square::G1) == of({Square::E2, Square::F3, Square::H3}));
}

TEST_CASE("king attacks", "[attacks]") {
  CHECK(king_attacks(Square::A1) == of({Square::A2, Square::B1, Square::B2}));
  CHECK(popcount(king_attacks(Square::E4)) == 8);
  CHECK(popcount(king_attacks(Square::E1)) == 5);
  CHECK(popcount(king_attacks(Square::H8)) == 3);
}

TEST_CASE("pawn attacks do not wrap around the board", "[attacks]") {
  CHECK(pawn_attacks(Color::White, Square::E4) == of({Square::D5, Square::F5}));
  CHECK(pawn_attacks(Color::Black, Square::E5) == of({Square::D4, Square::F4}));
  CHECK(pawn_attacks(Color::White, Square::A2) == bb(Square::B3));
  CHECK(pawn_attacks(Color::White, Square::H2) == bb(Square::G3));
  CHECK(pawn_attacks(Color::Black, Square::H7) == bb(Square::G6));
  CHECK(pawn_attacks(Color::Black, Square::A7) == bb(Square::B6));
  CHECK(pawn_attacks(Color::White, Square::E8) == 0);  // no square beyond the last rank
  CHECK(pawn_attacks(Color::Black, Square::E1) == 0);
}

TEST_CASE("rook attacks", "[attacks]") {
  CHECK(popcount(rook_attacks(Square::D4, 0)) == 14);
  CHECK(popcount(rook_attacks(Square::A1, 0)) == 14);

  // Blockers on d6 and b4: the rook sees up to and including them, nothing beyond.
  const Bitboard occ = of({Square::D6, Square::B4});
  const Bitboard attacks = rook_attacks(Square::D4, occ);
  CHECK((attacks & bb(Square::D6)) != 0);
  CHECK((attacks & bb(Square::D7)) == 0);
  CHECK((attacks & bb(Square::B4)) != 0);
  CHECK((attacks & bb(Square::A4)) == 0);
  CHECK((attacks & bb(Square::D1)) != 0);
  CHECK((attacks & bb(Square::H4)) != 0);
}

TEST_CASE("bishop attacks", "[attacks]") {
  CHECK(popcount(bishop_attacks(Square::D4, 0)) == 13);
  CHECK(popcount(bishop_attacks(Square::A1, 0)) == 7);
  // Bishop on c1 with b2 and d2 occupied sees only those two squares.
  CHECK(bishop_attacks(Square::C1, of({Square::B2, Square::D2})) == of({Square::B2, Square::D2}));
}

TEST_CASE("queen attacks are rook plus bishop", "[attacks]") {
  const Bitboard occ = of({Square::C3, Square::F6, Square::D7});
  CHECK(queen_attacks(Square::D4, occ) == (rook_attacks(Square::D4, occ) | bishop_attacks(Square::D4, occ)));
  CHECK(popcount(queen_attacks(Square::D4, 0)) == 27);
}

TEST_CASE("ray loops agree with an independent slider on random occupancies", "[attacks]") {
  std::uint64_t state = 0x123456789ABCDEFULL;  // xorshift64: fixed seed, reproducible
  auto next = [&state] {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  };
  for (int round = 0; round < 2000; ++round) {
    const Bitboard occ = next() & next();  // sparse-ish occupancy
    for (int s = 0; s < 64; ++s) {
      const Square sq = sq_from_num(s);
      REQUIRE(rook_attacks(sq, occ) == reference_slide(sq, occ, {1, -1, 8, -8}));
      REQUIRE(bishop_attacks(sq, occ) == reference_slide(sq, occ, {7, -7, 9, -9}));
    }
  }
}
