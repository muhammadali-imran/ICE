#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "attacks.h"
#include "board.h"
#include "movegen.h"

using namespace ice;

namespace {

// Finds the legal move with the given UCI text, or the null move if there is none.
Move find_move(Board& board, const std::string& uci) {
  MoveList list;
  generate_legal(board, list);
  for (const Move m : list)
    if (m.to_uci() == uci) return m;
  return Move();
}

// Plays a sequence of UCI moves, failing the test if any is illegal.
void play(Board& board, const std::vector<std::string>& moves) {
  for (const std::string& uci : moves) {
    const Move m = find_move(board, uci);
    INFO("playing " << uci << " from " << board.fen());
    REQUIRE_FALSE(m.is_null());
    board.make_move(m);
  }
}

// Visits every position reachable in `depth` legal plies. After each make the
// incremental key must equal the from-scratch key; after each unmake the whole
// position must be exactly what it was.
void check_make_unmake(Board& board, int depth) {
  if (depth == 0) return;
  const std::string fen_before = board.fen();
  const Key key_before = board.key();
  MoveList list;
  generate_legal(board, list);
  for (const Move m : list) {
    board.make_move(m);
    REQUIRE(board.key() == board.compute_key());
    check_make_unmake(board, depth - 1);
    board.unmake_move();
    REQUIRE(board.fen() == fen_before);
    REQUIRE(board.key() == key_before);
  }
}

const char* const kRoundTripFens[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",  // en passant square
    "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",
    "8/8/8/8/8/8/8/K6k w - - 0 1",                       // bare kings, no castling
    "r3k3/8/8/8/8/8/8/4K2R w Kq - 12 57",                // partial rights, counters
    "4k3/8/8/8/8/8/8/4K3 b - - 99 150",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b - - 0 1",
};

}  // namespace

TEST_CASE("a new board is the starting position", "[board]") {
  const Board board;
  CHECK(board.fen() == Board::kStartFen);
  CHECK(board.side_to_move() == Color::White);
  CHECK(board.castling_rights() == kAllCastling);
  CHECK(board.ep_square() == Square::None);
  CHECK(popcount(board.occupied()) == 32);
  CHECK(popcount(board.pieces(Color::White, PieceType::Pawn)) == 8);
  CHECK(board.piece_on(Square::E1) == Piece::WhiteKing);
  CHECK(board.piece_on(Square::D8) == Piece::BlackQueen);
  CHECK(board.piece_on(Square::E4) == Piece::None);
  CHECK(board.king_square(Color::Black) == Square::E8);
  CHECK(board.key() == board.compute_key());
  CHECK_FALSE(board.in_check());
}

TEST_CASE("FEN round trip is exact", "[board][fen]") {
  for (const char* fen : kRoundTripFens) {
    Board board;
    std::string error;
    INFO(fen);
    REQUIRE(board.set_fen(fen, &error));
    CHECK(board.fen() == fen);
    CHECK(board.key() == board.compute_key());
  }
}

TEST_CASE("FEN counters are optional", "[board][fen]") {
  Board board;
  REQUIRE(board.set_fen("8/8/8/8/8/8/8/K6k w - -"));
  CHECK(board.fen() == "8/8/8/8/8/8/8/K6k w - - 0 1");
  REQUIRE(board.set_fen("8/8/8/8/8/8/8/K6k b - - 5"));
  CHECK(board.fen() == "8/8/8/8/8/8/8/K6k b - - 5 1");
}

TEST_CASE("malformed or impossible FENs are rejected and leave the board unchanged", "[board][fen]") {
  const char* const bad[] = {
      "",
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq",          // too few fields
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 x",  // too many fields
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",             // 7 ranks
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/8 w KQkq - 0 1",  // 9 ranks
      "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",    // bad digit
      "rnbqkbnr/pppppppp/7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",    // short rank
      "rnbqkbnrr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",   // long rank
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",    // unknown piece
      "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",    // no black king
      "rnbqkknr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",    // two black kings
      "Pnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",    // pawn on rank 8
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNp w KQkq - 0 1",    // pawn on rank 1
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",    // bad side
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkz - 0 1",    // bad castling char
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KKkq - 0 1",    // duplicate right
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN1 w KQkq - 0 1",    // K right but no rook on h1
      "4k3/8/8/8/8/8/8/4K3 w K - 0 1",                                // K right, no rook
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e4 0 1",    // ep on wrong rank
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e6 0 1",    // no pawn behind ep square
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq z9 0 1",    // not a square
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - -1 1",    // negative clock
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0",     // move number 0
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - x 1",     // not a number
      "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1",                              // black king attacked, white to move
  };
  for (const char* fen : bad) {
    Board board;
    REQUIRE(board.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    const std::string before = board.fen();
    std::string error;
    INFO("FEN: '" << fen << "'");
    CHECK_FALSE(board.set_fen(fen, &error));
    CHECK_FALSE(error.empty());
    CHECK(board.fen() == before);
    CHECK(board.key() == board.compute_key());
  }
}

TEST_CASE("check detection", "[board]") {
  Board board;
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1"));
  CHECK(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/3r4/4K3 w - - 0 1"));
  CHECK_FALSE(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/1b6/8/8/4K3 w - - 0 1"));  // bishop b4 attacks e1 through c3,d2
  CHECK(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/8/3n4/8/4K3 w - - 0 1"));  // knight d3 attacks e1
  CHECK(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/3p4/4K3 w - - 0 1"));  // black pawn d2 attacks e1
  CHECK(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/5p2/4K3 w - - 0 1"));  // black pawn f2 attacks e1 too
  CHECK(board.in_check());
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/4p3/4K3 w - - 0 1"));  // pawn straight ahead: no check
  CHECK_FALSE(board.in_check());
}

TEST_CASE("Zobrist keys: transpositions, side to move and en passant", "[board][zobrist]") {
  // Same position through two different move orders.
  Board a;
  Board b;
  play(a, {"g1f3", "g8f6", "b1c3", "b8c6"});
  play(b, {"b1c3", "b8c6", "g1f3", "g8f6"});
  CHECK(a.fen() == b.fen());
  CHECK(a.key() == b.key());

  // Same pieces, different side to move: different key.
  Board white;
  Board black;
  REQUIRE(white.set_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1"));
  REQUIRE(black.set_fen("4k3/8/8/8/8/8/8/4K3 b - - 0 1"));
  CHECK(white.key() != black.key());

  // An en passant square nobody can use must not change the key...
  Board with_ep;
  Board without_ep;
  REQUIRE(with_ep.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
  REQUIRE(without_ep.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"));
  CHECK(with_ep.key() == without_ep.key());

  // ...but one that can be used must.
  Board usable_ep;
  Board usable_no_ep;
  REQUIRE(usable_ep.set_fen("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
  REQUIRE(usable_no_ep.set_fen("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"));
  CHECK(usable_ep.key() != usable_no_ep.key());

  // Different castling rights: different key.
  Board all_rights;
  Board no_rights;
  REQUIRE(all_rights.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
  REQUIRE(no_rights.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"));
  CHECK(all_rights.key() != no_rights.key());
}

TEST_CASE("make and unmake restore the position and keep the Zobrist key exact", "[board][zobrist]") {
  // Positions chosen to exercise castling, en passant, promotions and captures.
  const char* const fens[] = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",
  };
  for (const char* fen : fens) {
    Board board;
    REQUIRE(board.set_fen(fen));
    INFO(fen);
    check_make_unmake(board, 3);
    CHECK(board.history_size() == 0);
  }
}

TEST_CASE("en passant removes the captured pawn and unmake puts it back", "[board][moves]") {
  Board board;
  REQUIRE(board.set_fen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3"));
  const Move ep = find_move(board, "e5d6");
  REQUIRE_FALSE(ep.is_null());
  CHECK(ep.is_en_passant());
  board.make_move(ep);
  CHECK(board.piece_on(Square::D6) == Piece::WhitePawn);
  CHECK(board.piece_on(Square::D5) == Piece::None);  // captured pawn is gone
  CHECK(board.piece_on(Square::E5) == Piece::None);
  CHECK(board.fen() == "rnbqkbnr/ppp1pppp/3P4/8/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 3");
  board.unmake_move();
  CHECK(board.fen() == "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
}

TEST_CASE("a double push sets the en passant square", "[board][moves]") {
  Board board;
  play(board, {"e2e4"});
  CHECK(board.ep_square() == Square::E3);
  CHECK(board.fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  play(board, {"g8f6"});
  CHECK(board.ep_square() == Square::None);
  CHECK(board.fen() == "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2");
}

TEST_CASE("castling moves the rook and removes the rights", "[board][moves]") {
  Board board;
  REQUIRE(board.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));

  const Move short_castle = find_move(board, "e1g1");
  REQUIRE_FALSE(short_castle.is_null());
  CHECK(short_castle.flags() == Move::KingCastle);
  board.make_move(short_castle);
  CHECK(board.fen() == "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
  board.unmake_move();
  CHECK(board.fen() == "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

  const Move long_castle = find_move(board, "e1c1");
  REQUIRE_FALSE(long_castle.is_null());
  CHECK(long_castle.flags() == Move::QueenCastle);
  board.make_move(long_castle);
  CHECK(board.fen() == "r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");

  // Black castles kingside in reply. (Queenside would be illegal: the white rook on
  // d1 now attacks d8, a square the black king would pass through.)
  CHECK(find_move(board, "e8c8").is_null());
  const Move black_castle = find_move(board, "e8g8");
  REQUIRE_FALSE(black_castle.is_null());
  board.make_move(black_castle);
  CHECK(board.fen() == "r4rk1/8/8/8/8/8/8/2KR3R w - - 2 2");
}

TEST_CASE("moving a rook or capturing one removes only the matching castling right", "[board][moves]") {
  Board board;
  REQUIRE(board.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
  play(board, {"a1a2"});
  CHECK(board.castling_rights() == (kWhiteKingSide | kBlackKingSide | kBlackQueenSide));

  REQUIRE(board.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
  play(board, {"h1h8"});  // captures the black h8 rook
  CHECK(board.castling_rights() == (kWhiteQueenSide | kBlackQueenSide));

  REQUIRE(board.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
  play(board, {"e1e2"});
  CHECK(board.castling_rights() == (kBlackKingSide | kBlackQueenSide));
}

TEST_CASE("castling is not generated through, out of, or into check, or through pieces", "[board][moves]") {
  Board board;
  // Black rook attacks f1: no king-side castling, queen-side still fine.
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"));
  CHECK_FALSE(find_move(board, "e1g1").is_null());
  REQUIRE(board.set_fen("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1"));
  CHECK(find_move(board, "e1g1").is_null());
  CHECK_FALSE(find_move(board, "e1c1").is_null());
  // King in check: no castling at all.
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1"));
  CHECK(find_move(board, "e1g1").is_null());
  CHECK(find_move(board, "e1c1").is_null());
  // Piece in the way.
  REQUIRE(board.set_fen("4k3/8/8/8/8/8/8/RN2K1NR w KQ - 0 1"));
  CHECK(find_move(board, "e1g1").is_null());
  CHECK(find_move(board, "e1c1").is_null());
  // An attacked b1 does not stop queen-side castling (only the king's path matters).
  REQUIRE(board.set_fen("1r2k3/8/8/8/8/8/8/R3K3 w Q - 0 1"));
  CHECK_FALSE(find_move(board, "e1c1").is_null());
}

TEST_CASE("promotion replaces the pawn and unmake restores it", "[board][moves]") {
  Board board;
  REQUIRE(board.set_fen("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1"));
  MoveList list;
  generate_legal(board, list);
  int promotions = 0;
  for (const Move m : list)
    if (m.is_promotion()) ++promotions;
  CHECK(promotions == 8);  // a8 (4 pieces) and b8xN (4 pieces)

  const Move queen = find_move(board, "a7a8q");
  REQUIRE_FALSE(queen.is_null());
  board.make_move(queen);
  CHECK(board.piece_on(Square::A8) == Piece::WhiteQueen);
  CHECK(board.piece_on(Square::A7) == Piece::None);
  board.unmake_move();
  CHECK(board.piece_on(Square::A7) == Piece::WhitePawn);
  CHECK(board.piece_on(Square::A8) == Piece::None);

  const Move capture = find_move(board, "a7b8n");
  REQUIRE_FALSE(capture.is_null());
  CHECK(capture.is_capture());
  board.make_move(capture);
  CHECK(board.piece_on(Square::B8) == Piece::WhiteKnight);
  board.unmake_move();
  CHECK(board.piece_on(Square::B8) == Piece::BlackKnight);
  CHECK(board.fen() == "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
}

TEST_CASE("pinned pieces cannot expose the king", "[board][moves]") {
  Board board;
  REQUIRE(board.set_fen("4k3/4r3/8/8/8/8/4N3/4K3 w - - 0 1"));  // knight e2 pinned by rook e7
  MoveList list;
  generate_legal(board, list);
  for (const Move m : list) CHECK(board.piece_on(m.from()) == Piece::WhiteKing);
}

TEST_CASE("en passant that would expose the king along the rank is illegal", "[board][moves]") {
  Board board;
  // White king a5, black rook h5; white pawn b5 and black pawn c5 stand between them.
  // Taking en passant (b5xc6) would remove both pawns from the rank and expose the king.
  REQUIRE(board.set_fen("4k3/8/8/KPp4r/8/8/8/8 w - c6 0 1"));
  CHECK(find_move(board, "b5c6").is_null());
  CHECK_FALSE(find_move(board, "b5b6").is_null());
}

TEST_CASE("checkmate and stalemate positions have no legal moves", "[board][moves]") {
  Board board;
  MoveList list;
  REQUIRE(board.set_fen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"));  // fool's mate
  generate_legal(board, list);
  CHECK(list.empty());
  CHECK(board.in_check());

  list = MoveList();
  REQUIRE(board.set_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));  // stalemate
  generate_legal(board, list);
  CHECK(list.empty());
  CHECK_FALSE(board.in_check());
}

TEST_CASE("the diagram shows the board and the FEN", "[board]") {
  const Board board;
  const std::string text = board.diagram();
  CHECK(text.find("8 | r | n | b | q | k | b | n | r |") != std::string::npos);
  CHECK(text.find("1 | R | N | B | Q | K | B | N | R |") != std::string::npos);
  CHECK(text.find("a   b   c   d   e   f   g   h") != std::string::npos);
  CHECK(text.find(std::string("Fen: ") + std::string(Board::kStartFen)) != std::string::npos);
}
