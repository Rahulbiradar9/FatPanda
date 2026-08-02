#pragma once

#include "board/board.hpp"
#include "move/move.hpp"
#include <cstdint>
#include <vector>

#include <atomic>
#include <chrono>

namespace ChessEngine {

// Global search stop and time limit control variables
extern std::atomic<bool> g_stop_search;
extern std::chrono::steady_clock::time_point g_start_time;
extern int g_time_limit_ms;

// Constants for Search bounds
constexpr int MATE_SCORE = 30000;
constexpr int INFINITY_SCORE = 50000;
constexpr int MAX_PLY = 64;

// Struct to track stats and context during a search
struct SearchInfo {
    uint64_t nodes_searched = 0;
    Move pv_move = MOVE_NONE; // Best move from the previous iterative deepening iteration
    Move pv_table[MAX_PLY][MAX_PLY] = {};
    int pv_length[MAX_PLY] = {};
    Move killer_moves[2][MAX_PLY] = {};
    int history_moves[12][64] = {};
};

// Struct containing the result of a search
struct SearchResult {
    Move best_move = MOVE_NONE;
    int score = 0;
    uint64_t nodes_searched = 0;
    int search_depth = 0;
    std::vector<Move> pv;
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
