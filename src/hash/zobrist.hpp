#pragma once

#include <cstdint>
#include "utils/chess_types.hpp"

namespace ChessEngine {

// Zobrist keys arrays
extern uint64_t piece_keys[12][64];
extern uint64_t castling_keys[16];
extern uint64_t en_passant_keys[64];
extern uint64_t side_key;

// Initialize Zobrist keys using a fixed-seed pseudorandom number generator
void initialize_zobrist_keys();

} // namespace ChessEngine
