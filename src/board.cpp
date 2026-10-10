#include "board.h"

#include <cassert>
#include <charconv>

#include "attacks.h"
#include "zobrist.h"

namespace ice {

namespace {

// For each square, the castling rights that survive a move from or to that square.
// Moving the king or a rook from its home square (or capturing a rook there) removes
// the matching rights, so make_move() only needs `rights &= mask[from] & mask[to]`.
constexpr std::array<std::uint8_t, kSquareCount> make_castling_masks() {
  std::array<std::uint8_t, kSquareCount> m{};
  for (auto& v : m) v = kAllCastling;
  m[idx(Square::A1)] = kAllCastling & static_cast<std::uint8_t>(~kWhiteQueenSide);
  m[idx(Square::H1)] = kAllCastling & static_cast<std::uint8_t>(~kWhiteKingSide);
  m[idx(Square::E1)] = kAllCastling & static_cast<std::uint8_t>(~(kWhiteKingSide | kWhiteQueenSide));
  m[idx(Square::A8)] = kAllCastling & static_cast<std::uint8_t>(~kBlackQueenSide);
  m[idx(Square::H8)] = kAllCastling & static_cast<std::uint8_t>(~kBlackKingSide);
  m[idx(Square::E8)] = kAllCastling & static_cast<std::uint8_t>(~(kBlackKingSide | kBlackQueenSide));
  return m;
}
constexpr auto kCastlingMask = make_castling_masks();

constexpr std::string_view kPieceChars = "PNBRQKpnbrqk";

Piece piece_from_char(char c) noexcept {
  const auto pos = kPieceChars.find(c);
  return pos == std::string_view::npos ? Piece::None : static_cast<Piece>(pos);
}

// Splits `text` on single spaces into at most `fields.size()` pieces. Returns how many
// fields were found, or fields.size() + 1 if there were too many.
std::size_t split_fields(std::string_view text, std::array<std::string_view, 6>& fields) {
  std::size_t n = 0;
  std::size_t pos = 0;
  while (pos < text.size()) {
    while (pos < text.size() && text[pos] == ' ') ++pos;
    if (pos >= text.size()) break;
    const std::size_t end = text.find(' ', pos);
    const std::size_t stop = end == std::string_view::npos ? text.size() : end;
    if (n == fields.size()) return n + 1;
    fields[n++] = text.substr(pos, stop - pos);
    pos = stop;
  }
  return n;
}

bool parse_int(std::string_view text, int& out) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

}  // namespace

Board::Board() { set_start_position(); }

void Board::set_start_position() {
  [[maybe_unused]] const bool ok = set_fen(kStartFen);
  assert(ok);
}

void Board::clear() noexcept {
  pieces_ = PieceBoards{};
  by_color_ = {};
  occupied_ = 0;
  mailbox_.fill(Piece::None);
  side_ = Color::White;
  castling_ = 0;
  ep_ = Square::None;
  halfmove_ = 0;
  fullmove_ = 1;
  key_ = 0;
  history_size_ = 0;
}

// ------------------------------------------------------------ put / remove

// These two update every redundant representation and the Zobrist key at once, so
// the rest of the code can never get them out of sync.
void Board::put_piece(Piece p, Square s) noexcept {
  assert(mailbox_[idx(s)] == Piece::None);
  const Bitboard square = bb(s);
  pieces_[color_of(p), type_of(p)] |= square;
  by_color_[idx(color_of(p))] |= square;
  occupied_ |= square;
  mailbox_[idx(s)] = p;
  key_ ^= zobrist::kKeys.piece[idx(p)][idx(s)];
}

void Board::remove_piece(Square s) noexcept {
  const Piece p = mailbox_[idx(s)];
  assert(p != Piece::None);
  const Bitboard square = bb(s);
  pieces_[color_of(p), type_of(p)] &= ~square;
  by_color_[idx(color_of(p))] &= ~square;
  occupied_ &= ~square;
  mailbox_[idx(s)] = Piece::None;
  key_ ^= zobrist::kKeys.piece[idx(p)][idx(s)];
}

// ------------------------------------------------------------------ queries

bool Board::is_attacked(Square s, Color by) const noexcept {
  // Work backwards: a pawn of `by` attacks `s` exactly when a pawn of the *other*
  // color standing on `s` would attack that pawn's square.
  if ((pawn_attacks(~by, s) & pieces_[by, PieceType::Pawn]) != 0) return true;
  if ((knight_attacks(s) & pieces_[by, PieceType::Knight]) != 0) return true;
  if ((king_attacks(s) & pieces_[by, PieceType::King]) != 0) return true;
  const Bitboard diagonal = pieces_[by, PieceType::Bishop] | pieces_[by, PieceType::Queen];
  if ((bishop_attacks(s, occupied_) & diagonal) != 0) return true;
  const Bitboard straight = pieces_[by, PieceType::Rook] | pieces_[by, PieceType::Queen];
  return (rook_attacks(s, occupied_) & straight) != 0;
}

bool Board::ep_hashed() const noexcept {
  if (ep_ == Square::None) return false;
  // The side to move can capture en passant if one of its pawns attacks the ep square.
  return (pawn_attacks(~side_, ep_) & pieces_[side_, PieceType::Pawn]) != 0;
}

Key Board::compute_key() const noexcept {
  Key key = 0;
  Bitboard occ = occupied_;
  while (occ != 0) {
    const Square s = pop_lsb(occ);
    key ^= zobrist::kKeys.piece[idx(mailbox_[idx(s)])][idx(s)];
  }
  key ^= zobrist::kKeys.castling[castling_];
  if (ep_hashed()) key ^= zobrist::kKeys.ep_file[static_cast<std::size_t>(file_of(ep_))];
  if (side_ == Color::Black) key ^= zobrist::kKeys.side;
  return key;
}

// -------------------------------------------------------------- make/unmake

void Board::make_move(Move m) noexcept {
  assert(history_size_ < kMaxHistory);
  const Square from = m.from();
  const Square to = m.to();
  const Piece piece = mailbox_[idx(from)];
  assert(piece != Piece::None && color_of(piece) == side_);
  const Color us = side_;

  Undo& undo = history_[history_size_++];
  undo.move = m;
  undo.captured = Piece::None;
  undo.castling = castling_;
  undo.ep = ep_;
  undo.halfmove = halfmove_;
  undo.key = key_;

  // Take the old en passant and castling facts out of the hash; the new ones go in at
  // the end. (XOR twice cancels, so this is "remove old, add new".)
  if (ep_hashed()) key_ ^= zobrist::kKeys.ep_file[static_cast<std::size_t>(file_of(ep_))];
  key_ ^= zobrist::kKeys.castling[castling_];
  ep_ = Square::None;

  // 1. Remove a captured piece. For en passant it is not on the destination square.
  if (m.is_en_passant()) {
    const Square captured_square = sq_from_num(sq_num(to) + (us == Color::White ? -8 : 8));
    undo.captured = mailbox_[idx(captured_square)];
    remove_piece(captured_square);
  } else if (m.is_capture()) {
    undo.captured = mailbox_[idx(to)];
    remove_piece(to);
  }

  // 2. Move the piece (a promoting pawn is replaced by the new piece).
  remove_piece(from);
  put_piece(m.is_promotion() ? make_piece(us, m.promotion_type()) : piece, to);

  // 3. Castling also moves the rook.
  if (m.flags() == Move::KingCastle) {
    const int rank = rank_of(from);
    remove_piece(make_square(7, rank));
    put_piece(make_piece(us, PieceType::Rook), make_square(5, rank));
  } else if (m.flags() == Move::QueenCastle) {
    const int rank = rank_of(from);
    remove_piece(make_square(0, rank));
    put_piece(make_piece(us, PieceType::Rook), make_square(3, rank));
  }

  // 4. A double push leaves an en passant square behind it.
  if (m.flags() == Move::DoublePush) ep_ = sq_from_num(sq_num(from) + (us == Color::White ? 8 : -8));

  // 5. Bookkeeping.
  castling_ = static_cast<std::uint8_t>(castling_ & kCastlingMask[idx(from)] & kCastlingMask[idx(to)]);
  halfmove_ = (type_of(piece) == PieceType::Pawn || m.is_capture()) ? 0 : halfmove_ + 1;
  if (us == Color::Black) ++fullmove_;
  side_ = ~side_;

  key_ ^= zobrist::kKeys.side;
  key_ ^= zobrist::kKeys.castling[castling_];
  if (ep_hashed()) key_ ^= zobrist::kKeys.ep_file[static_cast<std::size_t>(file_of(ep_))];
}

void Board::unmake_move() noexcept {
  assert(history_size_ > 0);
  const Undo& undo = history_[--history_size_];
  const Move m = undo.move;
  const Square from = m.from();
  const Square to = m.to();

  side_ = ~side_;  // back to the side that made the move
  const Color us = side_;
  if (us == Color::Black) --fullmove_;

  // Reverse step 3, 2, 1 of make_move().
  if (m.flags() == Move::KingCastle) {
    const int rank = rank_of(from);
    remove_piece(make_square(5, rank));
    put_piece(make_piece(us, PieceType::Rook), make_square(7, rank));
  } else if (m.flags() == Move::QueenCastle) {
    const int rank = rank_of(from);
    remove_piece(make_square(3, rank));
    put_piece(make_piece(us, PieceType::Rook), make_square(0, rank));
  }

  const Piece moved = mailbox_[idx(to)];
  remove_piece(to);
  put_piece(m.is_promotion() ? make_piece(us, PieceType::Pawn) : moved, from);

  if (m.is_en_passant()) {
    put_piece(undo.captured, sq_from_num(sq_num(to) + (us == Color::White ? -8 : 8)));
  } else if (m.is_capture()) {
    put_piece(undo.captured, to);
  }

  castling_ = undo.castling;
  ep_ = undo.ep;
  halfmove_ = undo.halfmove;
  key_ = undo.key;  // put_piece/remove_piece touched the key; restore it exactly
}

// ---------------------------------------------------------------------- FEN

bool Board::set_fen(std::string_view fen, std::string* error) {
  auto fail = [error](const char* message) {
    if (error != nullptr) *error = message;
    return false;
  };

  std::array<std::string_view, 6> fields{};
  const std::size_t count = split_fields(fen, fields);
  if (count < 4) return fail("FEN needs at least 4 fields");
  if (count > 6) return fail("FEN has more than 6 fields");

  // ---- Field 1: piece placement, parsed into a plain array first so a bad FEN
  //      leaves the board untouched.
  std::array<Piece, kSquareCount> placement;
  placement.fill(Piece::None);
  {
    int rank = 7;
    int file = 0;
    for (const char c : fields[0]) {
      if (c == '/') {
        if (file != 8) return fail("rank does not have 8 squares");
        if (--rank < 0) return fail("too many ranks");
        file = 0;
      } else if (c >= '1' && c <= '8') {
        file += c - '0';
        if (file > 8) return fail("rank has more than 8 squares");
      } else {
        const Piece p = piece_from_char(c);
        if (p == Piece::None) return fail("unknown piece character");
        if (file >= 8) return fail("rank has more than 8 squares");
        placement[idx(make_square(file, rank))] = p;
        ++file;
      }
    }
    if (rank != 0 || file != 8) return fail("FEN does not describe 8 ranks of 8 squares");
  }

  std::array<int, kPieceCount> counts{};
  for (std::size_t s = 0; s < kSquareCount; ++s) {
    const Piece p = placement[s];
    if (p == Piece::None) continue;
    ++counts[idx(p)];
    if (type_of(p) == PieceType::Pawn && (s < 8 || s >= 56)) return fail("pawn on the first or last rank");
  }
  if (counts[idx(Piece::WhiteKing)] != 1 || counts[idx(Piece::BlackKing)] != 1)
    return fail("each side needs exactly one king");

  // ---- Field 2: side to move.
  Color side;
  if (fields[1] == "w") side = Color::White;
  else if (fields[1] == "b") side = Color::Black;
  else return fail("side to move must be w or b");

  // ---- Field 3: castling rights. Each right needs the king and rook at home.
  std::uint8_t castling = 0;
  if (fields[2] != "-") {
    for (const char c : fields[2]) {
      std::uint8_t right = 0;
      Piece king = Piece::None;
      Piece rook = Piece::None;
      Square king_sq = Square::None;
      Square rook_sq = Square::None;
      switch (c) {
        case 'K': right = kWhiteKingSide; king = Piece::WhiteKing; rook = Piece::WhiteRook; king_sq = Square::E1; rook_sq = Square::H1; break;
        case 'Q': right = kWhiteQueenSide; king = Piece::WhiteKing; rook = Piece::WhiteRook; king_sq = Square::E1; rook_sq = Square::A1; break;
        case 'k': right = kBlackKingSide; king = Piece::BlackKing; rook = Piece::BlackRook; king_sq = Square::E8; rook_sq = Square::H8; break;
        case 'q': right = kBlackQueenSide; king = Piece::BlackKing; rook = Piece::BlackRook; king_sq = Square::E8; rook_sq = Square::A8; break;
        default: return fail("unknown castling character");
      }
      if ((castling & right) != 0) return fail("duplicate castling right");
      if (placement[idx(king_sq)] != king || placement[idx(rook_sq)] != rook)
        return fail("castling right without king and rook on their home squares");
      castling = static_cast<std::uint8_t>(castling | right);
    }
  }

  // ---- Field 4: en passant square. After a double push it must be on rank 3 (Black
  //      to move) or rank 6 (White to move), with the pushed pawn in front of it.
  Square ep = Square::None;
  if (fields[3] != "-") {
    ep = parse_square(fields[3]);
    if (ep == Square::None) return fail("bad en passant square");
    if (rank_of(ep) != (side == Color::White ? 5 : 2)) return fail("en passant square on the wrong rank");
    const Square pawn_sq = sq_from_num(sq_num(ep) + (side == Color::White ? -8 : 8));
    if (placement[idx(pawn_sq)] != make_piece(~side, PieceType::Pawn)) return fail("no pawn behind the en passant square");
    if (placement[idx(ep)] != Piece::None) return fail("en passant square is occupied");
  }

  // ---- Fields 5 and 6: move counters (optional).
  int halfmove = 0;
  int fullmove = 1;
  if (count >= 5 && (!parse_int(fields[4], halfmove) || halfmove < 0)) return fail("bad halfmove clock");
  if (count >= 6 && (!parse_int(fields[5], fullmove) || fullmove < 1)) return fail("bad move number");

  // ---- Commit. Keep a copy so we can roll back if the position turns out illegal.
  const Board backup = *this;
  clear();
  for (std::size_t s = 0; s < kSquareCount; ++s)
    if (placement[s] != Piece::None) put_piece(placement[s], sq_from_num(static_cast<int>(s)));
  side_ = side;
  castling_ = castling;
  ep_ = ep;
  halfmove_ = halfmove;
  fullmove_ = fullmove;

  // The side that just moved cannot have left its own king attacked.
  if (is_attacked(king_square(~side_), side_)) {
    *this = backup;
    return fail("the side not to move is in check");
  }

  key_ = compute_key();
  return true;
}

std::string Board::fen() const {
  std::string out;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      const Piece p = mailbox_[idx(make_square(file, rank))];
      if (p == Piece::None) {
        ++empty;
        continue;
      }
      if (empty > 0) out += static_cast<char>('0' + empty);
      empty = 0;
      out += kPieceChars[idx(p)];
    }
    if (empty > 0) out += static_cast<char>('0' + empty);
    if (rank > 0) out += '/';
  }
  out += side_ == Color::White ? " w " : " b ";
  if (castling_ == 0) {
    out += '-';
  } else {
    if ((castling_ & kWhiteKingSide) != 0) out += 'K';
    if ((castling_ & kWhiteQueenSide) != 0) out += 'Q';
    if ((castling_ & kBlackKingSide) != 0) out += 'k';
    if ((castling_ & kBlackQueenSide) != 0) out += 'q';
  }
  out += ' ';
  out += square_name(ep_);
  out += ' ';
  out += std::to_string(halfmove_);
  out += ' ';
  out += std::to_string(fullmove_);
  return out;
}

std::string Board::diagram() const {
  std::string out;
  for (int rank = 7; rank >= 0; --rank) {
    out += "  +---+---+---+---+---+---+---+---+\n";
    out += static_cast<char>('1' + rank);
    out += " |";
    for (int file = 0; file < 8; ++file) {
      const Piece p = mailbox_[idx(make_square(file, rank))];
      out += ' ';
      out += p == Piece::None ? ' ' : kPieceChars[idx(p)];
      out += " |";
    }
    out += '\n';
  }
  out += "  +---+---+---+---+---+---+---+---+\n";
  out += "    a   b   c   d   e   f   g   h\n\n";
  out += "Fen: " + fen() + '\n';
  return out;
}

}  // namespace ice
