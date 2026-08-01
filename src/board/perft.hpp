#pragma once

#include "board.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace ChessEngine {

struct PerftResult {
    uint64_t nodes;
    double time_ms;
    double nps;
};

// Compute perft recursively at the given depth
uint64_t runPerft(Board& board, int depth);

// Run divide mode: prints move-by-move node counts to stdout and returns the map
std::vector<std::pair<std::string, uint64_t>> runPerftDivide(Board& board, int depth, bool print_to_stdout = true);

// Run benchmark mode: runs perft at the given depth and measures duration/NPS
PerftResult runPerftBenchmark(Board& board, int depth);

} // namespace ChessEngine
