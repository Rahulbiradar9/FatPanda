#pragma once

#include <cstdint>
#include <bit>
#include "types.hpp"

namespace ChessEngine {

// Bitboard type alias
using Bitboard = uint64_t;

// Bitboard constants
constexpr Bitboard EMPTY_BOARD = 0ULL;
constexpr Bitboard FULL_BOARD = ~0ULL;

// --- Legacy Snake_case Bitboard Operations (Backward Compatibility) ---

// Set a bit on the bitboard for a given square
inline constexpr void set_bit(Bitboard& bb, Square sq) {
    if (sq != Square::None) {
        bb |= (1ULL << static_cast<uint8_t>(sq));
    }
}

// Clear a bit on the bitboard for a given square
inline constexpr void clear_bit(Bitboard& bb, Square sq) {
    if (sq != Square::None) {
        bb &= ~(1ULL << static_cast<uint8_t>(sq));
    }
}

// Test if a bit on the bitboard is set for a given square
inline constexpr bool test_bit(Bitboard bb, Square sq) {
    if (sq == Square::None) {
        return false;
    }
    return (bb & (1ULL << static_cast<uint8_t>(sq))) != 0;
}

// Count the number of active bits (popcount) on a bitboard
inline constexpr int count_bits(Bitboard bb) {
    return std::popcount(bb);
}

// Get the square of the least significant bit (LSB) and clear it from the bitboard
// Returns Square::None if the bitboard is empty
inline constexpr Square pop_lsb(Bitboard& bb) {
    if (bb == 0) {
        return Square::None;
    }
    int lsb_index = std::countr_zero(bb);
    bb &= bb - 1; // Clear the LSB
    return static_cast<Square>(lsb_index);
}

// Helper to get LSB square without clearing it
inline constexpr Square get_lsb(Bitboard bb) {
    if (bb == 0) {
        return Square::None;
    }
    return static_cast<Square>(std::countr_zero(bb));
}

// --- New CamelCase Bitboard Operations (New Requirements) ---

// Set Bit (Square overload)
inline constexpr void setBit(Bitboard& bb, Square sq) {
    if (sq != Square::None) {
        bb |= (1ULL << static_cast<uint8_t>(sq));
    }
}

// Set Bit (int overload)
inline constexpr void setBit(Bitboard& bb, int sq) {
    if (sq >= 0 && sq < 64) {
        bb |= (1ULL << sq);
    }
}

// Clear Bit (Square overload)
inline constexpr void clearBit(Bitboard& bb, Square sq) {
    if (sq != Square::None) {
        bb &= ~(1ULL << static_cast<uint8_t>(sq));
    }
}

// Clear Bit (int overload)
inline constexpr void clearBit(Bitboard& bb, int sq) {
    if (sq >= 0 && sq < 64) {
        bb &= ~(1ULL << sq);
    }
}

// Get Bit (Square overload)
inline constexpr bool getBit(Bitboard bb, Square sq) {
    if (sq == Square::None) return false;
    return (bb & (1ULL << static_cast<uint8_t>(sq))) != 0;
}

// Get Bit (int overload)
inline constexpr bool getBit(Bitboard bb, int sq) {
    if (sq < 0 || sq >= 64) return false;
    return (bb & (1ULL << sq)) != 0;
}

// Toggle Bit (Square overload)
inline constexpr void toggleBit(Bitboard& bb, Square sq) {
    if (sq != Square::None) {
        bb ^= (1ULL << static_cast<uint8_t>(sq));
    }
}

// Toggle Bit (int overload)
inline constexpr void toggleBit(Bitboard& bb, int sq) {
    if (sq >= 0 && sq < 64) {
        bb ^= (1ULL << sq);
    }
}

// Count Bits
inline constexpr int countBits(Bitboard bb) {
    return std::popcount(bb);
}

// Pop LSB
inline constexpr Square popLSB(Bitboard& bb) {
    if (bb == 0) return Square::None;
    int lsb_index = std::countr_zero(bb);
    bb &= bb - 1; // Clear the LSB
    return static_cast<Square>(lsb_index);
}

// LSB (returns Square)
inline constexpr Square lsb(Bitboard bb) {
    if (bb == 0) return Square::None;
    return static_cast<Square>(std::countr_zero(bb));
}

// MSB (returns Square)
inline constexpr Square msb(Bitboard bb) {
    if (bb == 0) return Square::None;
    return static_cast<Square>(63 - std::countl_zero(bb));
}

// Print Bitboard helper declaration
void printBitboard(Bitboard bb);

} // namespace ChessEngine
