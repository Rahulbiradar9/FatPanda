#pragma once

#include <array>
#include "types.hpp"
#include "bitboard.hpp"

namespace ChessEngine {

// Non-sliding attacks: Pawn, Knight, King (constexpr calculations for lookup tables)

constexpr Bitboard get_pawn_attacks_constexpr(Square sq, Color color) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;
    
    int rank = get_rank(sq);
    int file = get_file(sq);

    if (color == Color::White) {
        if (rank < 7) {
            if (file > 0) set_bit(attacks, make_square(file - 1, rank + 1));
            if (file < 7) set_bit(attacks, make_square(file + 1, rank + 1));
        }
    } else {
        if (rank > 0) {
            if (file > 0) set_bit(attacks, make_square(file - 1, rank - 1));
            if (file < 7) set_bit(attacks, make_square(file + 1, rank - 1));
        }
    }
    return attacks;
}

constexpr Bitboard get_knight_attacks_constexpr(Square sq) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;
    
    int rank = get_rank(sq);
    int file = get_file(sq);

    const int dr[] = { -2, -2, -1, -1, 1, 1, 2, 2 };
    const int df[] = { -1, 1, -2, 2, -2, 2, -1, 1 };

    for (int i = 0; i < 8; ++i) {
        int nr = rank + dr[i];
        int nf = file + df[i];
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            set_bit(attacks, make_square(nf, nr));
        }
    }
    return attacks;
}

constexpr Bitboard get_king_attacks_constexpr(Square sq) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;

    int rank = get_rank(sq);
    int file = get_file(sq);

    const int dr[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    const int df[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

    for (int i = 0; i < 8; ++i) {
        int nr = rank + dr[i];
        int nf = file + df[i];
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            set_bit(attacks, make_square(nf, nr));
        }
    }
    return attacks;
}

// Compile-time precalculated lookup tables struct
struct AttackTables {
    std::array<std::array<Bitboard, 64>, 2> pawn_attacks;
    std::array<Bitboard, 64> knight_attacks;
    std::array<Bitboard, 64> king_attacks;

    constexpr AttackTables() : pawn_attacks{}, knight_attacks{}, king_attacks{} {
        for (int sq = 0; sq < 64; ++sq) {
            Square square = static_cast<Square>(sq);
            pawn_attacks[0][sq] = get_pawn_attacks_constexpr(square, Color::White);
            pawn_attacks[1][sq] = get_pawn_attacks_constexpr(square, Color::Black);
            knight_attacks[sq] = get_knight_attacks_constexpr(square);
            king_attacks[sq] = get_king_attacks_constexpr(square);
        }
    }
};

// Global constexpr tables compiled directly into the binary
inline constexpr AttackTables ATTACK_TABLES;

// --- New CamelCase Attack Functions ---

inline constexpr Bitboard getPawnAttacks(Square sq, Color color) {
    if (sq == Square::None || color == Color::None) return EMPTY_BOARD;
    return ATTACK_TABLES.pawn_attacks[static_cast<size_t>(color)][static_cast<size_t>(sq)];
}

inline constexpr Bitboard getKnightAttacks(Square sq) {
    if (sq == Square::None) return EMPTY_BOARD;
    return ATTACK_TABLES.knight_attacks[static_cast<size_t>(sq)];
}

inline constexpr Bitboard getKingAttacks(Square sq) {
    if (sq == Square::None) return EMPTY_BOARD;
    return ATTACK_TABLES.king_attacks[static_cast<size_t>(sq)];
}

// Sliding attack generators computed on the fly via blocker scanning

inline constexpr Bitboard getBishopAttacks(Square sq, Bitboard occupancy) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;

    int r = get_rank(sq);
    int f = get_file(sq);

    // Diagonal Up-Right
    for (int rank = r + 1, file = f + 1; rank < 8 && file < 8; ++rank, ++file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Diagonal Up-Left
    for (int rank = r + 1, file = f - 1; rank < 8 && file >= 0; ++rank, --file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Diagonal Down-Right
    for (int rank = r - 1, file = f + 1; rank >= 0 && file < 8; --rank, ++file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Diagonal Down-Left
    for (int rank = r - 1, file = f - 1; rank >= 0 && file >= 0; --rank, --file) {
        Square target = make_square(file, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }

    return attacks;
}

inline constexpr Bitboard getRookAttacks(Square sq, Bitboard occupancy) {
    Bitboard attacks = EMPTY_BOARD;
    if (sq == Square::None) return attacks;

    int r = get_rank(sq);
    int f = get_file(sq);

    // Up
    for (int rank = r + 1; rank < 8; ++rank) {
        Square target = make_square(f, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Down
    for (int rank = r - 1; rank >= 0; --rank) {
        Square target = make_square(f, rank);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Right
    for (int file = f + 1; file < 8; ++file) {
        Square target = make_square(file, r);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }
    // Left
    for (int file = f - 1; file >= 0; --file) {
        Square target = make_square(file, r);
        set_bit(attacks, target);
        if (test_bit(occupancy, target)) break;
    }

    return attacks;
}

inline constexpr Bitboard getQueenAttacks(Square sq, Bitboard occupancy) {
    return getBishopAttacks(sq, occupancy) | getRookAttacks(sq, occupancy);
}

// --- Legacy Snake_case Attack Functions (Backward Compatibility) ---

inline constexpr Bitboard get_pawn_attacks(Square sq, Color color) {
    return getPawnAttacks(sq, color);
}

inline constexpr Bitboard get_knight_attacks(Square sq) {
    return getKnightAttacks(sq);
}

inline constexpr Bitboard get_king_attacks(Square sq) {
    return getKingAttacks(sq);
}

inline constexpr Bitboard get_bishop_attacks(Square sq, Bitboard occupancy) {
    return getBishopAttacks(sq, occupancy);
}

inline constexpr Bitboard get_rook_attacks(Square sq, Bitboard occupancy) {
    return getRookAttacks(sq, occupancy);
}

inline constexpr Bitboard get_queen_attacks(Square sq, Bitboard occupancy) {
    return getQueenAttacks(sq, occupancy);
}

} // namespace ChessEngine
