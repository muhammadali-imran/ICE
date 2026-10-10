// A move packed into 16 bits (spec decision D9).
//
//   bits  0..5   from square
//   bits  6..11  to square
//   bits 12..15  flags
//
// The flag values follow a common engine layout so that the questions we ask most
// are single bit tests:
//   bit 2 (value 4) set  -> the move captures something
//   bit 3 (value 8) set  -> the move is a promotion; low two bits pick the piece
#pragma once

#include <cstdint>
#include <string>

#include "types.h"

namespace ice {

class Move {
 public:
  enum Flag : std::uint16_t {
    Quiet = 0,
    DoublePush = 1,
    KingCastle = 2,
    QueenCastle = 3,
    Capture = 4,
    EnPassant = 5,
    PromoKnight = 8,
    PromoBishop = 9,
    PromoRook = 10,
    PromoQueen = 11,
    PromoKnightCapture = 12,
    PromoBishopCapture = 13,
    PromoRookCapture = 14,
    PromoQueenCapture = 15,
  };

  constexpr Move() noexcept = default;  // the "null move": a1a1, never legal
  constexpr Move(Square from, Square to, Flag flag = Quiet) noexcept
      : data_(static_cast<std::uint16_t>(sq_num(from) | (sq_num(to) << 6) | (flag << 12))) {}

  constexpr Square from() const noexcept { return sq_from_num(data_ & 0x3F); }
  constexpr Square to() const noexcept { return sq_from_num((data_ >> 6) & 0x3F); }
  constexpr Flag flags() const noexcept { return static_cast<Flag>(data_ >> 12); }

  constexpr bool is_null() const noexcept { return data_ == 0; }
  constexpr bool is_capture() const noexcept { return (flags() & 4) != 0; }
  constexpr bool is_promotion() const noexcept { return (flags() & 8) != 0; }
  constexpr bool is_en_passant() const noexcept { return flags() == EnPassant; }
  constexpr bool is_castle() const noexcept { return flags() == KingCastle || flags() == QueenCastle; }

  // Precondition: is_promotion(). Knight, Bishop, Rook or Queen.
  constexpr PieceType promotion_type() const noexcept {
    return static_cast<PieceType>(1 + (flags() & 3));
  }

  constexpr std::uint16_t raw() const noexcept { return data_; }
  friend constexpr bool operator==(Move a, Move b) noexcept = default;

  // Long algebraic notation as UCI uses it: "e2e4", "e7e8q". Null move: "0000".
  std::string to_uci() const {
    if (is_null()) return "0000";
    std::string s = square_name(from()) + square_name(to());
    if (is_promotion()) s += "nbrq"[flags() & 3];
    return s;
  }

 private:
  std::uint16_t data_ = 0;
};

}  // namespace ice
