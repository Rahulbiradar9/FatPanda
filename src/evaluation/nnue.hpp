#pragma once

#include "board/board.hpp"
#include "utils/chess_types.hpp"
#include <array>
#include <string>

namespace ChessEngine {

// UCI options to configure NNUE
extern bool g_use_nnue;
extern std::string g_nnue_file;

// Initialize or change the loaded NNUE evaluation file
bool nnue_load_file(const std::string& path);

// Perform NNUE inference and return score from side-to-move's perspective
int nnue_evaluate(const Board& board);

// Fallback to classical evaluation
int evaluate_classical(const Board& board);

// Recompute the entire accumulator from scratch
void nnue_recompute_accumulator(const Board& board, Accumulator& accum);

// Update accumulator incrementally
void nnue_update_accumulator(
    const Accumulator& prev,
    Accumulator& next,
    const std::array<std::pair<Piece, Square>, 3>& removed, int num_removed,
    const std::array<std::pair<Piece, Square>, 3>& added, int num_added
);

// Helpers to get feature index for White and Black perspective accumulators
inline int get_nnue_feature_white(Piece p, Square sq) {
    return static_cast<int>(p) * 64 + static_cast<int>(sq);
}

inline int get_nnue_feature_black(Piece p, Square sq) {
    int p_val = static_cast<int>(p);
    int mapped_p = (p_val < 6) ? (p_val + 6) : (p_val - 6);
    int mapped_sq = static_cast<int>(sq) ^ 56; // mirror vertically
    return mapped_p * 64 + mapped_sq;
}

} // namespace ChessEngine
