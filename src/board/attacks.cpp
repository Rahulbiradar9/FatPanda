#include "attacks.hpp"
#include <vector>
#include <immintrin.h>

namespace ChessEngine {

// Magic tables storage
static Bitboard s_bishop_table[5248];
static Bitboard s_rook_table[102400];

Bitboard g_bishop_masks[64];
Bitboard g_rook_masks[64];
uint64_t g_bishop_magics[64];
uint64_t g_rook_magics[64];
uint8_t g_bishop_shifts[64];
uint8_t g_rook_shifts[64];
const Bitboard* g_bishop_attacks[64];
const Bitboard* g_rook_attacks[64];

namespace {

// Classical ray-casting attack generators used during magic table population
Bitboard compute_bishop_attacks_ray(Square sq, Bitboard occupancy) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;

    int r = get_rank(sq);
    int f = get_file(sq);

    for (int rank = r + 1, file = f + 1; rank < 8 && file < 8; ++rank, ++file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int rank = r + 1, file = f - 1; rank < 8 && file >= 0; ++rank, --file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int rank = r - 1, file = f + 1; rank >= 0 && file < 8; --rank, ++file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int rank = r - 1, file = f - 1; rank >= 0 && file >= 0; --rank, --file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    return attacks;
}

Bitboard compute_rook_attacks_ray(Square sq, Bitboard occupancy) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;

    int r = get_rank(sq);
    int f = get_file(sq);

    for (int rank = r + 1; rank < 8; ++rank) {
        Square target = make_square(f, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int rank = r - 1; rank >= 0; --rank) {
        Square target = make_square(f, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int file = f + 1; file < 8; ++file) {
        Square target = make_square(file, r);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    for (int file = f - 1; file >= 0; --file) {
        Square target = make_square(file, r);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    return attacks;
}

// Compute relevant occupancy masks (excluding outer board edges)
Bitboard compute_bishop_mask(Square sq) {
    Bitboard mask = EMPTY_BOARD;
    int r = get_rank(sq);
    int f = get_file(sq);

    for (int rank = r + 1, file = f + 1; rank < 7 && file < 7; ++rank, ++file)
        set_bit(mask, make_square(file, rank));
    for (int rank = r + 1, file = f - 1; rank < 7 && file > 0; ++rank, --file)
        set_bit(mask, make_square(file, rank));
    for (int rank = r - 1, file = f + 1; rank > 0 && file < 7; --rank, ++file)
        set_bit(mask, make_square(file, rank));
    for (int rank = r - 1, file = f - 1; rank > 0 && file > 0; --rank, --file)
        set_bit(mask, make_square(file, rank));

    return mask;
}

Bitboard compute_rook_mask(Square sq) {
    Bitboard mask = EMPTY_BOARD;
    int r = get_rank(sq);
    int f = get_file(sq);

    for (int rank = r + 1; rank < 7; ++rank) set_bit(mask, make_square(f, rank));
    for (int rank = r - 1; rank > 0; --rank) set_bit(mask, make_square(f, rank));
    for (int file = f + 1; file < 7; ++file) set_bit(mask, make_square(file, r));
    for (int file = f - 1; file > 0; --file) set_bit(mask, make_square(file, r));

    return mask;
}

// XorShift64 state for deterministic fast magic number discovery
uint64_t xorshift64(uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

uint64_t random_magic_candidate(uint64_t& state) {
    return xorshift64(state) & xorshift64(state) & xorshift64(state);
}

void init_sliding_magics() {
    size_t bishop_offset = 0;
    size_t rook_offset = 0;

    // 1. Initialize Bishop Magic Tables
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        Bitboard mask = compute_bishop_mask(sq);
        g_bishop_masks[s] = mask;
        int bits = count_bits(mask);
        int num_occupancies = 1 << bits;
        g_bishop_shifts[s] = static_cast<uint8_t>(64 - bits);
        g_bishop_attacks[s] = &s_bishop_table[bishop_offset];

        // Collect all sub-occupancies and reference attacks
        std::vector<Bitboard> occupancies(num_occupancies);
        std::vector<Bitboard> reference_attacks(num_occupancies);

        Bitboard occ = 0;
        int count = 0;
        do {
            occupancies[count] = occ;
            reference_attacks[count] = compute_bishop_attacks_ray(sq, occ);
            count++;
            occ = (occ - mask) & mask;
        } while (occ != 0);

        // Find collision-free magic number
        uint64_t rng_state = 1070372ULL + static_cast<uint64_t>(s) * 9876543ULL;
        std::vector<Bitboard> used_attacks(num_occupancies);

        while (true) {
            uint64_t magic = random_magic_candidate(rng_state);
            if (count_bits((mask * magic) & 0xFF00000000000000ULL) < 6) continue;

            std::fill(used_attacks.begin(), used_attacks.end(), EMPTY_BOARD);
            bool fail = false;

            for (int i = 0; i < num_occupancies; ++i) {
                size_t idx = static_cast<size_t>((occupancies[i] * magic) >> g_bishop_shifts[s]);
                if (used_attacks[idx] == EMPTY_BOARD) {
                    used_attacks[idx] = reference_attacks[i];
                } else if (used_attacks[idx] != reference_attacks[i]) {
                    fail = true;
                    break;
                }
            }

            if (!fail) {
                g_bishop_magics[s] = magic;
                for (int i = 0; i < num_occupancies; ++i) {
                    size_t idx = static_cast<size_t>((occupancies[i] * magic) >> g_bishop_shifts[s]);
                    s_bishop_table[bishop_offset + idx] = reference_attacks[i];
                }
                break;
            }
        }

        bishop_offset += num_occupancies;
    }

    // 2. Initialize Rook Magic Tables
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        Bitboard mask = compute_rook_mask(sq);
        g_rook_masks[s] = mask;
        int bits = count_bits(mask);
        int num_occupancies = 1 << bits;
        g_rook_shifts[s] = static_cast<uint8_t>(64 - bits);
        g_rook_attacks[s] = &s_rook_table[rook_offset];

        std::vector<Bitboard> occupancies(num_occupancies);
        std::vector<Bitboard> reference_attacks(num_occupancies);

        Bitboard occ = 0;
        int count = 0;
        do {
            occupancies[count] = occ;
            reference_attacks[count] = compute_rook_attacks_ray(sq, occ);
            count++;
            occ = (occ - mask) & mask;
        } while (occ != 0);

        uint64_t rng_state = 8492048ULL + static_cast<uint64_t>(s) * 1234567ULL;
        std::vector<Bitboard> used_attacks(num_occupancies);

        while (true) {
            uint64_t magic = random_magic_candidate(rng_state);
            if (count_bits((mask * magic) & 0xFF00000000000000ULL) < 6) continue;

            std::fill(used_attacks.begin(), used_attacks.end(), EMPTY_BOARD);
            bool fail = false;

            for (int i = 0; i < num_occupancies; ++i) {
                size_t idx = static_cast<size_t>((occupancies[i] * magic) >> g_rook_shifts[s]);
                if (used_attacks[idx] == EMPTY_BOARD) {
                    used_attacks[idx] = reference_attacks[i];
                } else if (used_attacks[idx] != reference_attacks[i]) {
                    fail = true;
                    break;
                }
            }

            if (!fail) {
                g_rook_magics[s] = magic;
                for (int i = 0; i < num_occupancies; ++i) {
                    size_t idx = static_cast<size_t>((occupancies[i] * magic) >> g_rook_shifts[s]);
                    s_rook_table[rook_offset + idx] = reference_attacks[i];
                }
                break;
            }
        }

        rook_offset += num_occupancies;
    }
}

struct MagicInitializer {
    MagicInitializer() {
        init_sliding_magics();
    }
} s_magic_init;

} // namespace

void init_attacks() {
    // Ensuring initializer is referenced
    (void)s_magic_init;
}

} // namespace ChessEngine
