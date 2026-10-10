#include "movegen.h"

#include "attacks.h"

namespace ice {

namespace {

// One move from `from` to each square in `targets`; captures are flagged when the
// target holds an enemy piece.
void add_moves(Square from, Bitboard targets, Bitboard enemy, MoveList& list) {
  while (targets != 0) {
    const Square to = pop_lsb(targets);
    list.push(Move(from, to, (bb(to) & enemy) != 0 ? Move::Capture : Move::Quiet));
  }
}

// Four promotion moves (queen, rook, bishop, knight) from `from` to `to`.
void add_promotions(Square from, Square to, bool capture, MoveList& list) {
  const Move::Flag first = capture ? Move::PromoKnightCapture : Move::PromoKnight;
  for (std::uint16_t i = 0; i < 4; ++i) list.push(Move(from, to, static_cast<Move::Flag>(first + i)));
}

void generate_pawn_moves(const Board& board, MoveList& list) {
  const Color us = board.side_to_move();
  const Color them = ~us;
  const Bitboard pawns = board.pieces(us, PieceType::Pawn);
  const Bitboard enemy = board.pieces(them);
  const Bitboard empty = ~board.occupied();

  const int forward = us == Color::White ? 8 : -8;
  const Bitboard promotion_rank = us == Color::White ? kRank8 : kRank1;
  // The rank a pawn lands on after one step from its start rank.
  const Bitboard third_rank = us == Color::White ? kRank3 : kRank6;

  // ---- Pushes. Shifting all pawns at once handles every pawn in one operation.
  const Bitboard single = shift_forward(pawns, us) & empty;
  const Bitboard dbl = shift_forward(single & third_rank, us) & empty;

  Bitboard quiet = single & ~promotion_rank;
  while (quiet != 0) {
    const Square to = pop_lsb(quiet);
    list.push(Move(sq_from_num(sq_num(to) - forward), to, Move::Quiet));
  }
  Bitboard promoting = single & promotion_rank;
  while (promoting != 0) {
    const Square to = pop_lsb(promoting);
    add_promotions(sq_from_num(sq_num(to) - forward), to, false, list);
  }
  Bitboard doubles = dbl;
  while (doubles != 0) {
    const Square to = pop_lsb(doubles);
    list.push(Move(sq_from_num(sq_num(to) - 2 * forward), to, Move::DoublePush));
  }

  // ---- Captures, one pawn at a time (each pawn has its own attack squares).
  Bitboard movers = pawns;
  while (movers != 0) {
    const Square from = pop_lsb(movers);
    Bitboard targets = pawn_attacks(us, from) & enemy;
    while (targets != 0) {
      const Square to = pop_lsb(targets);
      if ((bb(to) & promotion_rank) != 0) {
        add_promotions(from, to, true, list);
      } else {
        list.push(Move(from, to, Move::Capture));
      }
    }
  }

  // ---- En passant: our pawns that attack the en passant square.
  if (board.ep_square() != Square::None) {
    Bitboard capturers = pawn_attacks(them, board.ep_square()) & pawns;
    while (capturers != 0) list.push(Move(pop_lsb(capturers), board.ep_square(), Move::EnPassant));
  }
}

void generate_castling(const Board& board, MoveList& list) {
  const Color us = board.side_to_move();
  const Color them = ~us;
  const Bitboard occ = board.occupied();
  const std::uint8_t rights = board.castling_rights();

  if (us == Color::White) {
    if ((rights & kWhiteKingSide) != 0 && (occ & (bb(Square::F1) | bb(Square::G1))) == 0 &&
        !board.is_attacked(Square::E1, them) && !board.is_attacked(Square::F1, them) &&
        !board.is_attacked(Square::G1, them))
      list.push(Move(Square::E1, Square::G1, Move::KingCastle));
    if ((rights & kWhiteQueenSide) != 0 &&
        (occ & (bb(Square::B1) | bb(Square::C1) | bb(Square::D1))) == 0 &&
        !board.is_attacked(Square::E1, them) && !board.is_attacked(Square::D1, them) &&
        !board.is_attacked(Square::C1, them))
      list.push(Move(Square::E1, Square::C1, Move::QueenCastle));
  } else {
    if ((rights & kBlackKingSide) != 0 && (occ & (bb(Square::F8) | bb(Square::G8))) == 0 &&
        !board.is_attacked(Square::E8, them) && !board.is_attacked(Square::F8, them) &&
        !board.is_attacked(Square::G8, them))
      list.push(Move(Square::E8, Square::G8, Move::KingCastle));
    if ((rights & kBlackQueenSide) != 0 &&
        (occ & (bb(Square::B8) | bb(Square::C8) | bb(Square::D8))) == 0 &&
        !board.is_attacked(Square::E8, them) && !board.is_attacked(Square::D8, them) &&
        !board.is_attacked(Square::C8, them))
      list.push(Move(Square::E8, Square::C8, Move::QueenCastle));
  }
}

}  // namespace

void generate_pseudo_legal(const Board& board, MoveList& list) {
  const Color us = board.side_to_move();
  const Bitboard own = board.pieces(us);
  const Bitboard enemy = board.pieces(~us);
  const Bitboard occ = board.occupied();

  generate_pawn_moves(board, list);

  // Every non-pawn piece: attacks from its square, minus squares holding our own pieces.
  for (const PieceType type : {PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen,
                               PieceType::King}) {
    Bitboard movers = board.pieces(us, type);
    while (movers != 0) {
      const Square from = pop_lsb(movers);
      add_moves(from, piece_attacks(type, from, occ) & ~own, enemy, list);
    }
  }

  generate_castling(board, list);
}

void generate_legal(Board& board, MoveList& list) {
  const Color us = board.side_to_move();
  MoveList pseudo;
  generate_pseudo_legal(board, pseudo);
  for (const Move m : pseudo) {
    board.make_move(m);
    if (!board.is_attacked(board.king_square(us), ~us)) list.push(m);
    board.unmake_move();
  }
}

}  // namespace ice
