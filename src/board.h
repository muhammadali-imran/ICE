// The chess position and the rules for changing it.
//
// A Board keeps the same facts in a few redundant forms because each is fastest for a
// different question:
//   * pieces_[color, type]  one bitboard per piece kind ("where are the white knights?")
//   * by_color_ / occupied_ unions of those ("where are all white pieces / any piece?")
//   * mailbox_[square]      "which piece stands on e4?" in O(1), needed by make/unmake
//                           and, later, by move ordering
// make_move() and unmake_move() keep all of them (and the Zobrist key) in sync.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "bitboard.h"
#include "move.h"
#include "types.h"

namespace ice {

// One bitboard per (color, piece type). Uses the C++23 multidimensional subscript:
// `pieces[Color::White, PieceType::Knight]`.
class PieceBoards {
 public:
  constexpr Bitboard& operator[](Color c, PieceType t) noexcept { return boards_[slot(c, t)]; }
  constexpr Bitboard operator[](Color c, PieceType t) const noexcept { return boards_[slot(c, t)]; }

 private:
  static constexpr std::size_t slot(Color c, PieceType t) noexcept {
    return idx(c) * kPieceTypeCount + idx(t);
  }
  std::array<Bitboard, kPieceCount> boards_{};
};

// Everything unmake_move() needs to restore what make_move() changed.
struct Undo {
  Move move;
  Piece captured = Piece::None;
  std::uint8_t castling = 0;
  Square ep = Square::None;
  int halfmove = 0;
  Key key = 0;
};

class Board {
 public:
  // Longest game plus search depth we can undo. Fixed size, so make/unmake never
  // allocates (spec section 3.2).
  static constexpr std::size_t kMaxHistory = 2048;

  static constexpr std::string_view kStartFen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  Board();  // standard starting position

  // ---- Setup and printing ------------------------------------------------
  void set_start_position();

  // Parses a FEN string. Returns false and leaves the board unchanged if the FEN is
  // malformed or describes an impossible position; `error` (if given) then says why.
  // Both the halfmove clock and the move number are optional and default to 0 and 1.
  bool set_fen(std::string_view fen, std::string* error = nullptr);

  std::string fen() const;
  // A small ASCII picture of the board for the `d` debug command.
  std::string diagram() const;

  // ---- Queries -----------------------------------------------------------
  Color side_to_move() const noexcept { return side_; }
  Square ep_square() const noexcept { return ep_; }
  std::uint8_t castling_rights() const noexcept { return castling_; }
  int halfmove_clock() const noexcept { return halfmove_; }
  int fullmove_number() const noexcept { return fullmove_; }
  Key key() const noexcept { return key_; }
  std::size_t history_size() const noexcept { return history_size_; }

  Bitboard pieces(Color c, PieceType t) const noexcept { return pieces_[c, t]; }
  Bitboard pieces(Color c) const noexcept { return by_color_[idx(c)]; }
  Bitboard occupied() const noexcept { return occupied_; }
  Piece piece_on(Square s) const noexcept { return mailbox_[idx(s)]; }
  Square king_square(Color c) const noexcept { return lsb(pieces_[c, PieceType::King]); }

  // Is square `s` attacked by any piece of color `by`?
  bool is_attacked(Square s, Color by) const noexcept;
  // The side to move is in check.
  bool in_check() const noexcept { return is_attacked(king_square(side_), ~side_); }

  // The Zobrist key computed from scratch. key() must always equal this; tests check
  // it after every make and unmake.
  Key compute_key() const noexcept;

  // ---- Changing the position --------------------------------------------
  // `m` must be pseudo-legal for the side to move (it may leave the own king in
  // check; the caller decides whether to keep it, see perft()).
  void make_move(Move m) noexcept;
  // Undoes the most recent make_move().
  void unmake_move() noexcept;

 private:
  void clear() noexcept;
  void put_piece(Piece p, Square s) noexcept;
  void remove_piece(Square s) noexcept;
  // True when an en passant capture is actually possible. Only then does the en
  // passant file take part in the hash, so equal positions always hash equal.
  bool ep_hashed() const noexcept;

  PieceBoards pieces_;
  std::array<Bitboard, kColorCount> by_color_{};
  Bitboard occupied_ = 0;
  std::array<Piece, kSquareCount> mailbox_{};

  Color side_ = Color::White;
  std::uint8_t castling_ = 0;
  Square ep_ = Square::None;  // the square a pawn skipped over, shown in FEN even if no capture is possible
  int halfmove_ = 0;
  int fullmove_ = 1;
  Key key_ = 0;

  std::array<Undo, kMaxHistory> history_{};
  std::size_t history_size_ = 0;
};

}  // namespace ice
