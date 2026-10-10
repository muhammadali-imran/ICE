#include <catch2/catch_test_macros.hpp>

#include "move.h"

using ice::Move;
using ice::Square;

TEST_CASE("move packs and unpacks every from/to/flag combination", "[move]") {
  for (int from = 0; from < 64; ++from) {
    for (int to = 0; to < 64; ++to) {
      for (int flag = 0; flag < 16; ++flag) {
        const Move m(ice::sq_from_num(from), ice::sq_from_num(to), static_cast<Move::Flag>(flag));
        REQUIRE(ice::sq_num(m.from()) == from);
        REQUIRE(ice::sq_num(m.to()) == to);
        REQUIRE(static_cast<int>(m.flags()) == flag);
      }
    }
  }
}

TEST_CASE("move flag queries", "[move]") {
  CHECK_FALSE(Move(Square::E2, Square::E4, Move::DoublePush).is_capture());
  CHECK(Move(Square::E4, Square::D5, Move::Capture).is_capture());
  CHECK(Move(Square::E5, Square::D6, Move::EnPassant).is_capture());
  CHECK(Move(Square::E5, Square::D6, Move::EnPassant).is_en_passant());
  CHECK(Move(Square::E1, Square::G1, Move::KingCastle).is_castle());
  CHECK(Move(Square::E1, Square::C1, Move::QueenCastle).is_castle());
  CHECK_FALSE(Move(Square::E1, Square::G1, Move::KingCastle).is_capture());

  const Move promo(Square::E7, Square::E8, Move::PromoQueen);
  CHECK(promo.is_promotion());
  CHECK_FALSE(promo.is_capture());
  CHECK(promo.promotion_type() == ice::PieceType::Queen);

  const Move promo_capture(Square::E7, Square::D8, Move::PromoKnightCapture);
  CHECK(promo_capture.is_promotion());
  CHECK(promo_capture.is_capture());
  CHECK(promo_capture.promotion_type() == ice::PieceType::Knight);

  CHECK(Move(Square::A7, Square::A8, Move::PromoBishop).promotion_type() == ice::PieceType::Bishop);
  CHECK(Move(Square::A7, Square::A8, Move::PromoRook).promotion_type() == ice::PieceType::Rook);
}

TEST_CASE("the default move is the null move", "[move]") {
  CHECK(Move().is_null());
  CHECK(Move().to_uci() == "0000");
  CHECK_FALSE(Move(Square::E2, Square::E4).is_null());
}

TEST_CASE("moves print in UCI long algebraic notation", "[move]") {
  CHECK(Move(Square::E2, Square::E4, Move::DoublePush).to_uci() == "e2e4");
  CHECK(Move(Square::G1, Square::F3).to_uci() == "g1f3");
  CHECK(Move(Square::E1, Square::G1, Move::KingCastle).to_uci() == "e1g1");
  CHECK(Move(Square::E7, Square::E8, Move::PromoQueen).to_uci() == "e7e8q");
  CHECK(Move(Square::E7, Square::E8, Move::PromoKnight).to_uci() == "e7e8n");
  CHECK(Move(Square::B7, Square::A8, Move::PromoRookCapture).to_uci() == "b7a8r");
  CHECK(Move(Square::B7, Square::A8, Move::PromoBishopCapture).to_uci() == "b7a8b");
}

TEST_CASE("squares map to names and back", "[types]") {
  CHECK(ice::square_name(Square::A1) == "a1");
  CHECK(ice::square_name(Square::H8) == "h8");
  CHECK(ice::square_name(Square::E4) == "e4");
  CHECK(ice::square_name(Square::None) == "-");
  CHECK(ice::parse_square("e4") == Square::E4);
  CHECK(ice::parse_square("a1") == Square::A1);
  CHECK(ice::parse_square("h8") == Square::H8);
  CHECK(ice::parse_square("i1") == Square::None);
  CHECK(ice::parse_square("a9") == Square::None);
  CHECK(ice::parse_square("e") == Square::None);
  CHECK(ice::parse_square("-") == Square::None);
  // Little-endian rank-file mapping from the spec: a1 = 0, h1 = 7, a2 = 8, h8 = 63.
  CHECK(ice::sq_num(Square::A1) == 0);
  CHECK(ice::sq_num(Square::H1) == 7);
  CHECK(ice::sq_num(Square::A2) == 8);
  CHECK(ice::sq_num(Square::H8) == 63);
}
