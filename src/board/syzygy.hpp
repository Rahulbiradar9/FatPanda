#pragma once

#include "board.hpp"
#include "move/move.hpp"
#include <string>

namespace ChessEngine {

// UCI configuration options for Syzygy tablebases
extern bool g_syzygy_enabled;
extern std::string g_syzygy_path;

// Initialize Syzygy tablebases from the given directory path
void syzygy_init(const std::string& path);

// Check if tablebases are successfully loaded and available
bool syzygy_is_loaded();

// Probe WDL (Win/Draw/Loss) for the current board position.
// Returns true on a tablebase hit, storing the translated score (e.g. DRAW_SCORE or mate-adjusted).
bool syzygy_probe_wdl(const Board& board, int& score);

// Probe Root for the optimal move at the root of the search.
// Returns true on a tablebase hit, storing the best move in best_move.
bool syzygy_probe_root(const Board& board, Move& best_move);

} // namespace ChessEngine
