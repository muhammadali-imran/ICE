// Zobrist hashing keys.
//
// A position's hash is the XOR of one random 64-bit number for every fact about it:
// each piece on each square, the castling rights, the en passant file (only when an
// en passant capture is really possible) and the side to move. Because XOR undoes
// itself, make/unmake can update the hash incrementally instead of recomputing it.
//
// The numbers come from a fixed seed, so every run (and every machine) gets the same
// keys. That keeps ICE deterministic (NFR2) and makes failures reproducible.
#pragma once

#include <array>
#include <cstdint>

#include "types.h"

namespace ice::zobrist {

struct Keys {
  std::array<std::array<Key, kSquareCount>, kPieceCount> piece{};
  std::array<Key, 16> castling{};  // indexed by the 4-bit castling-rights value
  std::array<Key, 8> ep_file{};
  Key side{};  // XORed in when Black is to move
};

// SplitMix64: a tiny, well-known generator with good statistical quality.
constexpr Key splitmix64(Key& state) noexcept {
  state += 0x9E3779B97F4A7C15ULL;
  Key z = state;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

constexpr Keys make_keys() noexcept {
  Keys k{};
  Key state = 0x4943453000000001ULL;  // "ICE0" followed by 1
  for (auto& per_piece : k.piece)
    for (auto& key : per_piece) key = splitmix64(state);
  for (auto& key : k.castling) key = splitmix64(state);
  for (auto& key : k.ep_file) key = splitmix64(state);
  k.side = splitmix64(state);
  return k;
}

// Computed at compile time.
inline constexpr Keys kKeys = make_keys();

}  // namespace ice::zobrist
