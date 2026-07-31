#pragma once

#include <array>
#include <cstdint>
#include "types.hpp"
#include "bitboard.hpp"

namespace ChessEngine {

// Castling rights bitmask constants
namespace Castling {
    constexpr uint8_t WK = 1;  // White King-side (O-O)
    constexpr uint8_t WQ = 2;  // White Queen-side (O-O-O)
    constexpr uint8_t BK = 4;  // Black King-side (O-O)
    constexpr uint8_t BQ = 8;  // Black Queen-side (O-O-O)
    constexpr uint8_t ALL = WK | WQ | BK | BQ;
    constexpr uint8_t NONE = 0;
}

class Board {
public:
    Board();

    // Reset the board to an empty state
    void clear();

    // Reset the board to the standard chess starting position
    void reset_to_start();

    // Place or remove a piece on a square (use Piece::None to remove)
    void set_piece(Square sq, Piece p);

    // Get the piece on a given square
    inline Piece get_piece(Square sq) const {
        if (!is_valid_square(sq)) return Piece::None;
        return board_squares_[static_cast<size_t>(sq)];
    }

    // Get the occupancy bitboard (Color::White, Color::Black, or Color::None for both)
    inline Bitboard get_occupancy(Color color) const {
        if (color == Color::None) {
            return occupancies_[2]; // both
        }
        return occupancies_[static_cast<size_t>(color)];
    }

    // Get the bitboard for a specific piece type and color
    inline Bitboard get_piece_bitboard(Piece piece) const {
        if (piece == Piece::None) return EMPTY_BOARD;
        return pieces_[static_cast<size_t>(piece)];
    }

    // Getters and Setters for game state
    inline Color get_side_to_move() const { return side_to_move_; }
    inline void set_side_to_move(Color color) { side_to_move_ = color; }

    inline uint8_t get_castling_rights() const { return castling_rights_; }
    inline void set_castling_rights(uint8_t rights) { castling_rights_ = rights; }

    inline Square get_en_passant() const { return en_passant_; }
    inline void set_en_passant(Square sq) { en_passant_ = sq; }

    inline int get_halfmove_clock() const { return halfmove_clock_; }
    inline void set_halfmove_clock(int clock) { halfmove_clock_ = clock; }

    inline int get_fullmove_number() const { return fullmove_number_; }
    inline void set_fullmove_number(int num) { fullmove_number_ = num; }

    // Print ASCII representation of the board to stdout for debugging
    void print() const;

private:
    // 12 Piece Bitboards
    std::array<Bitboard, 12> pieces_;

    // Occupancies: 0 = White, 1 = Black, 2 = Both
    std::array<Bitboard, 3> occupancies_;

    // Redundant array representation of the board for O(1) piece lookup
    std::array<Piece, 64> board_squares_;

    // Game state variables
    Color side_to_move_;
    uint8_t castling_rights_;
    Square en_passant_;
    int halfmove_clock_;
    int fullmove_number_;
};

} // namespace ChessEngine
