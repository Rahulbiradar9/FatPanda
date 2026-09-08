#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "types.hpp"
#include "bitboard.hpp"
#include "move/move.hpp"

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

struct UndoState {
    Square enPassant;
    uint8_t castlingRights;
    int halfmoveClock;
    int fullmoveNumber;
    Piece capturedPiece;
    uint64_t hashKey;
};

// The Accumulator represents the feature output of the first layer (transformer)
// from both White and Black perspectives.
struct Accumulator {
    std::array<int16_t, 256> hv[2]; // Index 0: White perspective, 1: Black perspective
};

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

    inline Square get_castling_rook(Color color, bool kingside) const {
        int idx = (color == Color::White ? 0 : 2) + (kingside ? 0 : 1);
        return castling_rooks_[idx];
    }

    inline void set_castling_rook(Color color, bool kingside, Square sq) {
        int idx = (color == Color::White ? 0 : 2) + (kingside ? 0 : 1);
        castling_rooks_[idx] = sq;
    }

    inline const std::array<Square, 4>& get_castling_rooks() const {
        return castling_rooks_;
    }

    inline void set_castling_rooks(const std::array<Square, 4>& rooks) {
        castling_rooks_ = rooks;
    }

    inline Square get_en_passant() const { return en_passant_; }
    inline void set_en_passant(Square sq) { en_passant_ = sq; }

    inline int get_halfmove_clock() const { return halfmove_clock_; }
    inline void set_halfmove_clock(int clock) { halfmove_clock_ = clock; }

    inline int get_fullmove_number() const { return fullmove_number_; }
    inline void set_fullmove_number(int num) { fullmove_number_ = num; }

    inline uint64_t get_hash_key() const { return hash_key_; }
    uint64_t compute_hash_key() const;

    // Print ASCII representation of the board to stdout for debugging
    void print() const;

    // Load board state from a FEN string. Returns true if parsing succeeded.
    bool load_from_fen(std::string_view fen);

    // Reconstruct FEN string from current board state
    std::string to_fen() const;

    // Make a move on the board. Returns false if the move leaves the king in check (illegal).
    bool make_move(Move m);

    // New CamelCase Board Operations (New Requirements)
    void setStartingPosition();
    Piece getPiece(Square sq) const;
    void placePiece(Square sq, Piece p);
    void removePiece(Square sq);
    void movePiece(Square from, Square to);
    void printBoard() const;
    bool loadFromFen(std::string_view fen);
    std::string toFen() const;
    bool makeMove(Move m, UndoState& undo);
    void unmakeMove(Move m, const UndoState& undo);
    bool isRepetition() const;
    bool is_insufficient_material() const;
    void makeNullMove(UndoState& undo);
    void unmakeNullMove(const UndoState& undo);

    inline const Accumulator& get_accumulator() const {
        return accum_history_[history_len_ - 1];
    }

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
    std::array<Square, 4> castling_rooks_ = { Square::H1, Square::A1, Square::H8, Square::A8 };
    Square en_passant_;
    int halfmove_clock_;
    int fullmove_number_;
    uint64_t hash_key_;

    // Repetition history tracking
    std::array<uint64_t, 1024> history_;
    int history_len_;

    // NNUE Accumulator History Stack
    std::vector<Accumulator> accum_history_;
};

extern bool g_chess960;

} // namespace ChessEngine
