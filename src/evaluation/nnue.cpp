#include "nnue.hpp"
#include "evaluation.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <immintrin.h>

namespace ChessEngine {

bool g_use_nnue = false;
std::string g_nnue_file = "nn.nnue";
static bool s_network_loaded = false;

// NNUE Network weights and biases
static alignas(32) std::array<std::array<int16_t, 256>, 768> w1;
static alignas(32) std::array<int16_t, 256> b1;
static std::array<std::array<int16_t, 16>, 512> w2;
static std::array<int16_t, 16> b2;
static std::array<int16_t, 16> w3;
static int16_t b3 = 0;

#if defined(__AVX2__)
inline void vec_add_256(int16_t* dst, const int16_t* src) {
    for (int i = 0; i < 256; i += 16) {
        __m256i d = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
        __m256i s = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_add_epi16(d, s));
    }
}

inline void vec_sub_256(int16_t* dst, const int16_t* src) {
    for (int i = 0; i < 256; i += 16) {
        __m256i d = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
        __m256i s = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_sub_epi16(d, s));
    }
}
#endif

// Helper function to initialize default weights
static void init_default_weights() {
    // Fill first layer weights with a simple piece material approximation
    for (int f = 0; f < 768; ++f) {
        int piece_type = f / 64;
        int val = 0;
        
        // WP=0, WN=1, WB=2, WR=3, WQ=4, WK=5, BP=6, BN=7, BB=8, BR=9, BQ=10, BK=11
        if (piece_type == 0) val = 10;
        else if (piece_type == 1) val = 30;
        else if (piece_type == 2) val = 32;
        else if (piece_type == 3) val = 50;
        else if (piece_type == 4) val = 90;
        else if (piece_type == 5) val = 10;
        else if (piece_type == 6) val = -10;
        else if (piece_type == 7) val = -30;
        else if (piece_type == 8) val = -32;
        else if (piece_type == 9) val = -50;
        else if (piece_type == 10) val = -90;
        else if (piece_type == 11) val = -10;
        
        for (int j = 0; j < 256; ++j) {
            w1[f][j] = static_cast<int16_t>(val);
        }
    }
    
    // Initialize biases to 0
    std::fill(b1.begin(), b1.end(), static_cast<int16_t>(0));
    
    // Initialize layer 2 weights to 1
    for (int i = 0; i < 512; ++i) {
        std::fill(w2[i].begin(), w2[i].end(), static_cast<int16_t>(1));
    }
    std::fill(b2.begin(), b2.end(), static_cast<int16_t>(0));
    
    // Initialize layer 3 weights to 1
    std::fill(w3.begin(), w3.end(), static_cast<int16_t>(1));
    b3 = 0;
    
    s_network_loaded = true;
}

// Struct to force default initialization on startup
struct NNUEInitializer {
    NNUEInitializer() {
        init_default_weights();
    }
} s_nnue_init;

bool nnue_load_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cout << "info string NNUE file " << path << " not found, using embedded default model." << std::endl;
        init_default_weights();
        return false;
    }
    
    // Read magic number: 0x4E4E5545 ("NNUE")
    uint32_t magic;
    if (!in.read(reinterpret_cast<char*>(&magic), 4) || magic != 0x4E4E5545) {
        std::cout << "info string Error: invalid NNUE file magic number" << std::endl;
        init_default_weights();
        return false;
    }
    
    // Read w1, b1, w2, b2, w3, b3 sequentially with error check
    if (!in.read(reinterpret_cast<char*>(&w1), sizeof(w1)) ||
        !in.read(reinterpret_cast<char*>(&b1), sizeof(b1)) ||
        !in.read(reinterpret_cast<char*>(&w2), sizeof(w2)) ||
        !in.read(reinterpret_cast<char*>(&b2), sizeof(b2)) ||
        !in.read(reinterpret_cast<char*>(&w3), sizeof(w3)) ||
        !in.read(reinterpret_cast<char*>(&b3), sizeof(b3))) {
        std::cout << "info string Error: truncated NNUE file" << std::endl;
        init_default_weights();
        return false;
    }
    
    s_network_loaded = true;
    std::cout << "info string NNUE model loaded successfully from: " << path << std::endl;
    return true;
}

void nnue_recompute_accumulator(const Board& board, Accumulator& accum) {
    // Fill accumulator with biases
    std::copy(b1.begin(), b1.end(), accum.hv[0].begin());
    std::copy(b1.begin(), b1.end(), accum.hv[1].begin());
    
    // Add active features for all pieces currently on the board
    for (int sq = 0; sq < 64; ++sq) {
        Piece p = board.get_piece(static_cast<Square>(sq));
        if (p != Piece::None) {
            int w_idx = get_nnue_feature_white(p, static_cast<Square>(sq));
            int b_idx = get_nnue_feature_black(p, static_cast<Square>(sq));
#if defined(__AVX2__)
            vec_add_256(accum.hv[0].data(), w1[w_idx].data());
            vec_add_256(accum.hv[1].data(), w1[b_idx].data());
#else
            for (int i = 0; i < 256; ++i) {
                accum.hv[0][i] += w1[w_idx][i];
                accum.hv[1][i] += w1[b_idx][i];
            }
#endif
        }
    }
}

void nnue_update_accumulator(
    const Accumulator& prev,
    Accumulator& next,
    const std::array<std::pair<Piece, Square>, 3>& removed, int num_removed,
    const std::array<std::pair<Piece, Square>, 3>& added, int num_added
) {
    // Copy the previous accumulator state
    next = prev;
    
    // Subtract features of removed pieces
    for (int r = 0; r < num_removed; ++r) {
        Piece p = removed[r].first;
        Square sq = removed[r].second;
        int w_idx = get_nnue_feature_white(p, sq);
        int b_idx = get_nnue_feature_black(p, sq);
#if defined(__AVX2__)
        vec_sub_256(next.hv[0].data(), w1[w_idx].data());
        vec_sub_256(next.hv[1].data(), w1[b_idx].data());
#else
        for (int i = 0; i < 256; ++i) {
            next.hv[0][i] -= w1[w_idx][i];
            next.hv[1][i] -= w1[b_idx][i];
        }
#endif
    }
    
    // Add features of added pieces
    for (int a = 0; a < num_added; ++a) {
        Piece p = added[a].first;
        Square sq = added[a].second;
        int w_idx = get_nnue_feature_white(p, sq);
        int b_idx = get_nnue_feature_black(p, sq);
#if defined(__AVX2__)
        vec_add_256(next.hv[0].data(), w1[w_idx].data());
        vec_add_256(next.hv[1].data(), w1[b_idx].data());
#else
        for (int i = 0; i < 256; ++i) {
            next.hv[0][i] += w1[w_idx][i];
            next.hv[1][i] += w1[b_idx][i];
        }
#endif
    }
}

int nnue_evaluate(const Board& board) {
    if (!s_network_loaded) {
        // Fall back to classical
        return evaluate_classical(board);
    }
    
    // Fetch active accumulator (from White and Black perspective history)
    const Accumulator& accum = board.get_accumulator();
    
    // Active-first perspective combination
    std::array<int16_t, 512> inputs;
    if (board.get_side_to_move() == Color::White) {
        // White perspective is inputs[0..255], Black is inputs[256..511]
        std::copy(accum.hv[0].begin(), accum.hv[0].end(), inputs.begin());
        std::copy(accum.hv[1].begin(), accum.hv[1].end(), inputs.begin() + 256);
    } else {
        // Black perspective is inputs[0..255], White is inputs[256..511]
        std::copy(accum.hv[1].begin(), accum.hv[1].end(), inputs.begin());
        std::copy(accum.hv[0].begin(), accum.hv[0].end(), inputs.begin() + 256);
    }
    
    // Hidden Layer 1 (512 -> 16 with standard ReLU)
    std::array<int32_t, 16> hidden;
    for (int j = 0; j < 16; ++j) {
        int32_t sum = b2[j];
        for (int i = 0; i < 512; ++i) {
            // Apply ReLU to inputs (std::max(0, val))
            int16_t activated_input = std::max(static_cast<int16_t>(0), inputs[i]);
            sum += activated_input * w2[i][j];
        }
        // Scale intermediate sum
        hidden[j] = std::max(0, sum / 64);
    }
    
    // Output Layer (16 -> 1)
    int32_t final_sum = b3;
    for (int i = 0; i < 16; ++i) {
        final_sum += hidden[i] * w3[i];
    }
    
    // Output score scaled back to centipawns
    int score = final_sum / 256;
    
    // Return score from side-to-move's perspective
    return score;
}

} // namespace ChessEngine
