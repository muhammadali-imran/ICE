// Perft ("performance test"): counts every legal move sequence of a given length.
// The counts for well-known positions are published, so matching them is the
// strongest available proof that move generation and make/unmake are correct.
#pragma once

#include <cstdint>
#include <iosfwd>

#include "board.h"

namespace ice {

// Number of legal move sequences of exactly `depth` plies from the current position.
std::uint64_t perft(Board& board, int depth);

// Like perft(), but prints the count below each root move ("e2e4: 600") and the total.
// When two engines disagree, comparing their divide output pinpoints the bad move.
std::uint64_t perft_divide(Board& board, int depth, std::ostream& out);

}  // namespace ice
