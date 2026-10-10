#include "perft.h"

#include <ostream>

#include "movegen.h"

namespace ice {

std::uint64_t perft(Board& board, int depth) {
  if (depth <= 0) return 1;

  const Color us = board.side_to_move();
  MoveList list;
  generate_pseudo_legal(board, list);

  std::uint64_t nodes = 0;
  for (const Move m : list) {
    board.make_move(m);
    // Keep the move only if it did not leave our own king attacked.
    if (!board.is_attacked(board.king_square(us), ~us)) nodes += depth == 1 ? 1 : perft(board, depth - 1);
    board.unmake_move();
  }
  return nodes;
}

std::uint64_t perft_divide(Board& board, int depth, std::ostream& out) {
  const Color us = board.side_to_move();
  MoveList list;
  generate_pseudo_legal(board, list);

  std::uint64_t total = 0;
  for (const Move m : list) {
    board.make_move(m);
    if (!board.is_attacked(board.king_square(us), ~us)) {
      const std::uint64_t nodes = depth <= 1 ? 1 : perft(board, depth - 1);
      out << m.to_uci() << ": " << nodes << '\n';
      total += nodes;
    }
    board.unmake_move();
  }
  out << "total: " << total << '\n';
  return total;
}

}  // namespace ice
