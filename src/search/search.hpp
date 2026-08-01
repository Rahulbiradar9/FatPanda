#pragma once

#include "board/board.hpp"
#include "move/move.hpp"
#include <cstdint>

namespace ChessEngine {

// Constants for Search bounds
constexpr int MATE_SCORE = 30000;
constexpr int INFINITY_SCORE = 50000;
constexpr int MAX_PLY = 64;

// Struct to track stats and context during a search
struct SearchInfo {
    uint64_t nodes_searched = 0;
    Move pv_move = MOVE_NONE; // Best move from the previous iterative deepening iteration
};

// Struct containing the result of a search
struct SearchResult {
    Move best_move = MOVE_NONE;
    int score = 0;
};

/**
 * Perform an iterative deepening chess search on a board up to the specified max depth.
 * Automatically handles Negamax, Alpha-Beta pruning, Quiescence search, and basic move ordering.
 * Logs search info in a UCI-compliant format.
 *
 * @param board The current state of the chess board.
 * @param max_depth The maximum search depth.
 * @return SearchResult containing the best move found and its evaluation score.
 */
SearchResult search(Board& board, int max_depth);

} // namespace ChessEngine
