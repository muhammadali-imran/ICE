// Basic chess types for ICE.
//
// Everything is an `enum class` (spec section 3.2) so a Color cannot be used as a
// Square by accident. `idx()` turns an enum into an array index, `sq_num()` turns a
// Square into a plain int for arithmetic.
//
// Square mapping (spec decision D4): little-endian rank-file, a1 = 0, b1 = 1, ...,
// h1 = 7, a2 = 8, ..., h8 = 63, so `square = rank * 8 + file`.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace ice {

using Bitboard = std::uint64_t;
using Key = std::uint64_t;  // Zobrist hash key

// ---------------------------------------------------------------- Color

enum class Color : std::uint8_t { White, Black };
inline constexpr std::size_t kColorCount = 2;

constexpr Color operator~(Color c) noexcept {
  return c == Color::White ? Color::Black : Color::White;
}
constexpr std::size_t idx(Color c) noexcept { return static_cast<std::size_t>(std::to_underlying(c)); }

// ------------------------------------------------------------ PieceType

enum class PieceType : std::uint8_t { Pawn, Knight, Bishop, Rook, Queen, King };
inline constexpr std::size_t kPieceTypeCount = 6;

constexpr std::size_t idx(PieceType t) noexcept { return static_cast<std::size_t>(std::to_underlying(t)); }

// ---------------------------------------------------------------- Piece

// A colored piece. White pieces come first so `color * 6 + type` is the index.
enum class Piece : std::uint8_t {
  WhitePawn, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
  BlackPawn, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
  None
};
inline constexpr std::size_t kPieceCount = 12;

constexpr std::size_t idx(Piece p) noexcept { return static_cast<std::size_t>(std::to_underlying(p)); }

constexpr Piece make_piece(Color c, PieceType t) noexcept {
  return static_cast<Piece>(idx(c) * kPieceTypeCount + idx(t));
}
// Precondition for both: p != Piece::None.
constexpr Color color_of(Piece p) noexcept { return idx(p) < kPieceTypeCount ? Color::White : Color::Black; }
constexpr PieceType type_of(Piece p) noexcept { return static_cast<PieceType>(idx(p) % kPieceTypeCount); }

// ---------------------------------------------------------------- Square

// clang-format off
enum class Square : std::uint8_t {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8,
  None
};
// clang-format on
inline constexpr int kSquareCount = 64;

constexpr std::size_t idx(Square s) noexcept { return static_cast<std::size_t>(std::to_underlying(s)); }
constexpr int sq_num(Square s) noexcept { return static_cast<int>(std::to_underlying(s)); }
constexpr Square sq_from_num(int n) noexcept { return static_cast<Square>(n); }

constexpr Square make_square(int file, int rank) noexcept { return sq_from_num(rank * 8 + file); }
constexpr int file_of(Square s) noexcept { return sq_num(s) & 7; }
constexpr int rank_of(Square s) noexcept { return sq_num(s) >> 3; }

// "e4" for Square::E4, "-" for Square::None.
inline std::string square_name(Square s) {
  if (s == Square::None) return "-";
  return {static_cast<char>('a' + file_of(s)), static_cast<char>('1' + rank_of(s))};
}

// Parses "e4". Returns Square::None if the text is not a square.
constexpr Square parse_square(std::string_view text) noexcept {
  if (text.size() != 2) return Square::None;
  const int file = text[0] - 'a';
  const int rank = text[1] - '1';
  if (file < 0 || file > 7 || rank < 0 || rank > 7) return Square::None;
  return make_square(file, rank);
}

// ------------------------------------------------------- Castling rights

// Four bits in one byte. Chess960 is not supported in v0, so a right always means
// "king on e1/e8 and rook on a1/h1 or a8/h8".
inline constexpr std::uint8_t kWhiteKingSide = 1;
inline constexpr std::uint8_t kWhiteQueenSide = 2;
inline constexpr std::uint8_t kBlackKingSide = 4;
inline constexpr std::uint8_t kBlackQueenSide = 8;
inline constexpr std::uint8_t kAllCastling = 15;

}  // namespace ice
