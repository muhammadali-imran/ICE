// Perft against the published reference values (spec section 8.1, from the Chess
// Programming Wiki "Perft Results" page). If these match, move generation and
// make/unmake are correct for castling, en passant, promotions, pins and checks.
//
// Tags: the default run covers depth 1..4. `[slow]` adds depth 5 (registered with
// CTest in Release builds) and `[.deep]` adds depth 6 for some positions; both are
// hidden by default and run with e.g.  ice_tests "[slow]".
#include <array>
#include <cstdint>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "board.h"
#include "perft.h"

namespace {

struct PerftCase {
  const char* name;
  const char* fen;
  // Node counts for depth 1..6; 0 means "not listed / too large to run".
  std::array<std::uint64_t, 6> nodes;
};

constexpr PerftCase kCases[] = {
    {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {20, 400, 8902, 197281, 4865609, 119060324}},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {48, 2039, 97862, 4085603, 193690690, 0}},
    {"position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {14, 191, 2812, 43238, 674624, 11030083}},
    {"position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {6, 264, 9467, 422333, 15833292, 706045033}},
    {"position 4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     {6, 264, 9467, 422333, 15833292, 706045033}},
    {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {44, 1486, 62379, 2103487, 89941194, 0}},
    {"position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {46, 2079, 89890, 3894594, 164075551, 0}},
};

void check_depth(int depth) {
  for (const PerftCase& c : kCases) {
    const std::uint64_t expected = c.nodes[static_cast<std::size_t>(depth - 1)];
    if (expected == 0) continue;
    ice::Board board;
    REQUIRE(board.set_fen(c.fen));
    INFO(c.name << " depth " << depth);
    CHECK(ice::perft(board, depth) == expected);
    // Perft must leave the position exactly as it found it.
    CHECK(board.fen() == c.fen);
    CHECK(board.key() == board.compute_key());
  }
}

}  // namespace

TEST_CASE("perft depth 1 to 4 matches the reference values", "[perft]") {
  for (int depth = 1; depth <= 4; ++depth) check_depth(depth);
}

TEST_CASE("perft depth 5 matches the reference values", "[perft][.slow]") { check_depth(5); }

TEST_CASE("perft depth 6 matches the reference values", "[perft][.deep]") { check_depth(6); }

TEST_CASE("perft depth 0 is one node", "[perft]") {
  ice::Board board;
  CHECK(ice::perft(board, 0) == 1);
}

TEST_CASE("perft divide totals match perft", "[perft]") {
  ice::Board board;
  std::ostringstream out;
  CHECK(ice::perft_divide(board, 3, out) == 8902);
  CHECK(out.str().find("e2e4: 600") != std::string::npos);  // the well-known divide value
  CHECK(out.str().find("total: 8902") != std::string::npos);
}
