// ICE command line entry point.
//
// v0.2 only has the developer commands below. The UCI loop arrives in v0.3 and will
// replace this file's argument handling.
//
//   ice perft <depth> [fen]    count legal move sequences (default: start position)
//   ice divide <depth> [fen]   perft with a count per root move
//   ice fen [fen]              parse a FEN, print the board and the FEN back
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "board.h"
#include "perft.h"

namespace {

void print_usage() {
  std::cerr << "usage:\n"
               "  ice perft <depth> [fen]\n"
               "  ice divide <depth> [fen]\n"
               "  ice fen [fen]\n";
}

// Joins argv[first..] with spaces so a FEN can be given with or without quotes.
std::string join_args(int argc, char** argv, int first) {
  std::string text;
  for (int i = first; i < argc; ++i) {
    if (!text.empty()) text += ' ';
    text += argv[i];
  }
  return text;
}

bool load_board(ice::Board& board, const std::string& fen) {
  if (fen.empty()) return true;  // keep the start position
  std::string error;
  if (board.set_fen(fen, &error)) return true;
  std::cerr << "invalid FEN: " << error << '\n';
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }
  const std::string_view command = argv[1];
  ice::Board board;

  if (command == "fen") {
    if (!load_board(board, join_args(argc, argv, 2))) return 1;
    std::cout << board.diagram();
    return 0;
  }

  if (command == "perft" || command == "divide") {
    if (argc < 3) {
      print_usage();
      return 2;
    }
    const int depth = std::atoi(argv[2]);
    if (depth < 0 || depth > 20) {
      std::cerr << "depth must be between 0 and 20\n";
      return 2;
    }
    if (!load_board(board, join_args(argc, argv, 3))) return 1;

    const auto start = std::chrono::steady_clock::now();
    const std::uint64_t nodes =
        command == "divide" ? ice::perft_divide(board, depth, std::cout) : ice::perft(board, depth);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    if (command == "perft") std::cout << "nodes: " << nodes << '\n';
    std::cout << "time: " << seconds << " s";
    if (seconds > 0) std::cout << "  (" << static_cast<std::uint64_t>(static_cast<double>(nodes) / seconds / 1e6) << " Mnodes/s)";
    std::cout << '\n';
    return 0;
  }

  print_usage();
  return 2;
}
