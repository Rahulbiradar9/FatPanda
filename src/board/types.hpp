#pragma once

#include <cstdint>
#include <string_view>

namespace ChessEngine {

// Foundational constants
constexpr int NUM_COLORS = 2;
constexpr int NUM_PIECE_TYPES = 6;
constexpr int NUM_SQUARES = 64;

// Chess Colors
enum class Color : uint8_t {
    White = 0,
    Black = 1,
    None = 2
};

// Operator to invert color (e.g. ~Color::White == Color::Black)
constexpr Color operator~(Color color) {
    if (color == Color::White) return Color::Black;
    if (color == Color::Black) return Color::White;
    return Color::None;
}

// Chess Piece Types
enum class PieceType : uint8_t {
    Pawn = 0,
    Knight = 1,
    Bishop = 2,
    Rook = 3,
    Queen = 4,
    King = 5,
    None = 6
};

// Chess Pieces (Combining Color and PieceType)
enum class Piece : uint8_t {
    WhitePawn = 0,
    WhiteKnight = 1,
    WhiteBishop = 2,
    WhiteRook = 3,
    WhiteQueen = 4,
    WhiteKing = 5,
    BlackPawn = 6,
    BlackKnight = 7,
    BlackBishop = 8,
    BlackRook = 9,
    BlackQueen = 10,
    BlackKing = 11,
    None = 12
};

// Create a Piece from Color and PieceType
constexpr Piece make_piece(Color color, PieceType type) {
    if (color == Color::None || type == PieceType::None) {
        return Piece::None;
    }
    return static_cast<Piece>((static_cast<uint8_t>(color) * 6) + static_cast<uint8_t>(type));
}

// Extract PieceType from Piece
constexpr PieceType get_piece_type(Piece piece) {
    if (piece == Piece::None) {
        return PieceType::None;
    }
    return static_cast<PieceType>(static_cast<uint8_t>(piece) % 6);
}

// Extract Color from Piece
constexpr Color get_piece_color(Piece piece) {
    if (piece == Piece::None) {
        return Color::None;
    }
    return static_cast<Color>(static_cast<uint8_t>(piece) / 6);
}

// Chess Squares (Little-Endian Rank-File mapping)
// A1 is 0, H1 is 7, A8 is 56, H8 is 63
enum class Square : uint8_t {
    A1 = 0, B1, C1, D1, E1, F1, G1, H1,
    A2 = 8, B2, C2, D2, E2, F2, G2, H2,
    A3 = 16, B3, C3, D3, E3, F3, G3, H3,
    A4 = 24, B4, C4, D4, E4, F4, G4, H4,
    A5 = 32, B5, C5, D5, E5, F5, G5, H5,
    A6 = 40, B6, C6, D6, E6, F6, G6, H6,
    A7 = 48, B7, C7, D7, E7, F7, G7, H7,
    A8 = 56, B8, C8, D8, E8, F8, G8, H8,
    None = 64
};

// Create a Square from file and rank indices (0-7)
constexpr Square make_square(int file, int rank) {
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return Square::None;
    }
    return static_cast<Square>((rank << 3) + file);
}

// Get file index (0 to 7 representing A to H)
constexpr int get_file(Square sq) {
    if (sq == Square::None) return -1;
    return static_cast<int>(sq) & 7;
}

// Get rank index (0 to 7 representing 1 to 8)
constexpr int get_rank(Square sq) {
    if (sq == Square::None) return -1;
    return static_cast<int>(sq) >> 3;
}

// Check if square is valid
constexpr bool is_valid_square(Square sq) {
    return sq != Square::None && static_cast<uint8_t>(sq) < 64;
}

} // namespace ChessEngine
