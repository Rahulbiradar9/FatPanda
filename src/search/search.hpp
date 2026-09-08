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
extern int g_num_threads;
extern std::chrono::steady_clock::time_point g_start_time;
extern int g_time_limit_soft_ms;
extern int g_time_limit_hard_ms;

// Constants for Search bounds
constexpr int MATE_SCORE = 30000;
constexpr int INFINITY_SCORE = 50000;
constexpr int MAX_PLY = 64;

struct MoveContext {
    Piece piece = Piece::None;
    Square to = Square::None;
};

// Correction history configuration
constexpr int CORRECTION_HISTORY_SIZE = 16384;
constexpr int CORRECTION_HISTORY_SCALE = 256;
constexpr int CORRECTION_HISTORY_MAX = 16384;

// Struct to track stats and context during a search
struct SearchInfo {
    uint64_t nodes_searched = 0;
    uint64_t tt_lookups = 0;
    uint64_t tt_hits = 0;
    Move pv_move = MOVE_NONE; // Best move from the previous iterative deepening iteration
    Move pv_table[MAX_PLY][MAX_PLY] = {};
    int pv_length[MAX_PLY] = {};
    Move killer_moves[2][MAX_PLY] = {};
    int history_moves[12][64] = {};
    int cont_history_1ply[12][64][12][64] = {};
    int cont_history_2ply[12][64][12][64] = {};
    int pawn_corr_hist[2][CORRECTION_HISTORY_SIZE] = {};
    int non_pawn_corr_hist[2][CORRECTION_HISTORY_SIZE] = {};
};

// Struct containing the result of a search
struct SearchResult {
    Move best_move = MOVE_NONE;
    int score = 0;
    uint64_t nodes_searched = 0;
    int search_depth = 0;
    std::vector<Move> pv;
    uint64_t tt_lookups = 0;
    uint64_t tt_hits = 0;
    double branching_factor = 0.0;
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
// Configuration toggles for advanced search optimizations
struct SearchSettings {
    bool pvs = true;
    bool nmp = true;
    bool lmr = true;
    bool aspiration = true;
    bool futility = true;
    bool rfp = true;
    bool see = true;
    bool singular = true;
    bool iir = true;
    bool lmp = true;
    bool probcut = true;
    bool corrhist = true;
};

extern SearchSettings g_search_settings;
extern int g_singular_margin;
extern int g_lmp_max_depth;
extern int g_probcut_margin;
extern int g_delta_margin;

SearchResult search_root(Board& board, int depth, SearchInfo& info, int alpha = -INFINITY_SCORE, int beta = INFINITY_SCORE);

SearchResult search(Board& board, int max_depth);

} // namespace ChessEngine
