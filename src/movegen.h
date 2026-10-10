// Move generation.
//
// generate_pseudo_legal() produces every move that obeys how the pieces move, but it
// may include moves that leave the mover's own king in check (spec decision D7).
// generate_legal() filters those out by making each move and testing the king.
#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "board.h"
#include "move.h"

namespace ice {

// A fixed-capacity list so generating moves never allocates. No chess position has
// more than 218 legal moves, so 256 is safe.
class MoveList {
 public:
  static constexpr std::size_t kCapacity = 256;

  void push(Move m) noexcept {
    assert(size_ < kCapacity);
    moves_[size_++] = m;
  }
  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }
  Move operator[](std::size_t i) const noexcept { return moves_[i]; }
  const Move* begin() const noexcept { return moves_.data(); }
  const Move* end() const noexcept { return moves_.data() + size_; }

 private:
  std::array<Move, kCapacity> moves_{};
  std::size_t size_ = 0;
};

// Appends all pseudo-legal moves for the side to move to `list`.
void generate_pseudo_legal(const Board& board, MoveList& list);

// Appends only the legal moves. Needs a mutable board because it tries each move.
void generate_legal(Board& board, MoveList& list);

}  // namespace ice
