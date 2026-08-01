#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ChessEngine {

// Strongly-typed Enums for Chess Engine
enum class Color : uint8_t {
    White = 0,
    Black = 1,
    None = 2
};

enum class PieceType : uint8_t {
    Pawn = 0,
    Knight = 1,
    Bishop = 2,
    Rook = 3,
    Queen = 4,
    King = 5,
    None = 6
};

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

enum class File : uint8_t {
    FileA = 0,
    FileB = 1,
    FileC = 2,
    FileD = 3,
    FileE = 4,
    FileF = 5,
    FileG = 6,
    FileH = 7,
    None = 8
};

enum class Rank : uint8_t {
    Rank1 = 0,
    Rank2 = 1,
    Rank3 = 2,
    Rank4 = 3,
    Rank5 = 4,
    Rank6 = 5,
    Rank7 = 6,
    Rank8 = 7,
    None = 8
};

// Bitboard type alias
using Bitboard = uint64_t;

// Constants for all 64 squares plus none
constexpr Square SQ_A1 = Square::A1; constexpr Square SQ_B1 = Square::B1; constexpr Square SQ_C1 = Square::C1; constexpr Square SQ_D1 = Square::D1;
constexpr Square SQ_E1 = Square::E1; constexpr Square SQ_F1 = Square::F1; constexpr Square SQ_G1 = Square::G1; constexpr Square SQ_H1 = Square::H1;
constexpr Square SQ_A2 = Square::A2; constexpr Square SQ_B2 = Square::B2; constexpr Square SQ_C2 = Square::C2; constexpr Square SQ_D2 = Square::D2;
constexpr Square SQ_E2 = Square::E2; constexpr Square SQ_F2 = Square::F2; constexpr Square SQ_G2 = Square::G2; constexpr Square SQ_H2 = Square::H2;
constexpr Square SQ_A3 = Square::A3; constexpr Square SQ_B3 = Square::B3; constexpr Square SQ_C3 = Square::C3; constexpr Square SQ_D3 = Square::D3;
constexpr Square SQ_E3 = Square::E3; constexpr Square SQ_F3 = Square::F3; constexpr Square SQ_G3 = Square::G3; constexpr Square SQ_H3 = Square::H3;
constexpr Square SQ_A4 = Square::A4; constexpr Square SQ_B4 = Square::B4; constexpr Square SQ_C4 = Square::C4; constexpr Square SQ_D4 = Square::D4;
constexpr Square SQ_E4 = Square::E4; constexpr Square SQ_F4 = Square::F4; constexpr Square SQ_G4 = Square::G4; constexpr Square SQ_H4 = Square::H4;
constexpr Square SQ_A5 = Square::A5; constexpr Square SQ_B5 = Square::B5; constexpr Square SQ_C5 = Square::C5; constexpr Square SQ_D5 = Square::D5;
constexpr Square SQ_E5 = Square::E5; constexpr Square SQ_F5 = Square::F5; constexpr Square SQ_G5 = Square::G5; constexpr Square SQ_H5 = Square::H5;
constexpr Square SQ_A6 = Square::A6; constexpr Square SQ_B6 = Square::B6; constexpr Square SQ_C6 = Square::C6; constexpr Square SQ_D6 = Square::D6;
constexpr Square SQ_E6 = Square::E6; constexpr Square SQ_F6 = Square::F6; constexpr Square SQ_G6 = Square::G6; constexpr Square SQ_H6 = Square::H6;
constexpr Square SQ_A7 = Square::A7; constexpr Square SQ_B7 = Square::B7; constexpr Square SQ_C7 = Square::C7; constexpr Square SQ_D7 = Square::D7;
constexpr Square SQ_E7 = Square::E7; constexpr Square SQ_F7 = Square::F7; constexpr Square SQ_G7 = Square::G7; constexpr Square SQ_H7 = Square::H7;
constexpr Square SQ_A8 = Square::A8; constexpr Square SQ_B8 = Square::B8; constexpr Square SQ_C8 = Square::C8; constexpr Square SQ_D8 = Square::D8;
constexpr Square SQ_E8 = Square::E8; constexpr Square SQ_F8 = Square::F8; constexpr Square SQ_G8 = Square::G8; constexpr Square SQ_H8 = Square::H8;
constexpr Square SQ_NONE = Square::None;

// Foundational helper functions
constexpr Color opposite(Color color) {
    if (color == Color::White) return Color::Black;
    if (color == Color::Black) return Color::White;
    return Color::None;
}

constexpr Color pieceColor(Piece piece) {
    if (piece == Piece::None) return Color::None;
    return static_cast<Color>(static_cast<uint8_t>(piece) / 6);
}

constexpr PieceType pieceType(Piece piece) {
    if (piece == Piece::None) return PieceType::None;
    return static_cast<PieceType>(static_cast<uint8_t>(piece) % 6);
}

std::string squareToString(Square sq);
Square stringToSquare(std::string_view str);

// Backward Compatibility Helpers (Mapping legacy APIs to the new type system)
constexpr Color operator~(Color color) {
    return opposite(color);
}

constexpr Color get_piece_color(Piece piece) {
    return pieceColor(piece);
}

constexpr PieceType get_piece_type(Piece piece) {
    return pieceType(piece);
}

constexpr Piece make_piece(Color color, PieceType type) {
    if (color == Color::None || type == PieceType::None) {
        return Piece::None;
    }
    return static_cast<Piece>((static_cast<uint8_t>(color) * 6) + static_cast<uint8_t>(type));
}

constexpr Square make_square(int file, int rank) {
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return Square::None;
    }
    return static_cast<Square>((rank << 3) + file);
}

constexpr int get_file(Square sq) {
    if (sq == Square::None) return -1;
    return static_cast<int>(sq) & 7;
}

constexpr int get_rank(Square sq) {
    if (sq == Square::None) return -1;
    return static_cast<int>(sq) >> 3;
}

constexpr bool is_valid_square(Square sq) {
    return sq != Square::None && static_cast<uint8_t>(sq) < 64;
}

// Foundational constants
constexpr int NUM_COLORS = 2;
constexpr int NUM_PIECE_TYPES = 6;
constexpr int NUM_SQUARES = 64;

} // namespace ChessEngine
