#include "zobrist.hpp"
#include <random>

namespace ChessEngine {

uint64_t piece_keys[12][64];
uint64_t castling_keys[16];
uint64_t en_passant_keys[64];
uint64_t side_key;

void initialize_zobrist_keys() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::mt19937_64 rng(1070372ULL);

    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            piece_keys[p][sq] = rng();
        }
    }

    for (int c = 0; c < 16; ++c) {
        castling_keys[c] = rng();
    }

    for (int sq = 0; sq < 64; ++sq) {
        en_passant_keys[sq] = rng();
    }

    side_key = rng();
}

} // namespace ChessEngine
