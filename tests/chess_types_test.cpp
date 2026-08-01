#include <gtest/gtest.h>
#include "utils/chess_types.hpp"

using namespace ChessEngine;

// Test enums are strongly typed and have expected values
TEST(ChessTypesTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(Color::White), 0);
    EXPECT_EQ(static_cast<int>(Color::Black), 1);
    EXPECT_EQ(static_cast<int>(Color::None), 2);

    EXPECT_EQ(static_cast<int>(PieceType::Pawn), 0);
    EXPECT_EQ(static_cast<int>(PieceType::King), 5);
    EXPECT_EQ(static_cast<int>(PieceType::None), 6);

    EXPECT_EQ(static_cast<int>(Piece::WhitePawn), 0);
    EXPECT_EQ(static_cast<int>(Piece::BlackKing), 11);
    EXPECT_EQ(static_cast<int>(Piece::None), 12);

    EXPECT_EQ(static_cast<int>(Square::A1), 0);
    EXPECT_EQ(static_cast<int>(Square::H8), 63);
    EXPECT_EQ(static_cast<int>(Square::None), 64);

    EXPECT_EQ(static_cast<int>(File::FileA), 0);
    EXPECT_EQ(static_cast<int>(File::FileH), 7);
    EXPECT_EQ(static_cast<int>(File::None), 8);

    EXPECT_EQ(static_cast<int>(Rank::Rank1), 0);
    EXPECT_EQ(static_cast<int>(Rank::Rank8), 7);
    EXPECT_EQ(static_cast<int>(Rank::None), 8);
}

// Test opposite Color helper
TEST(ChessTypesTest, OppositeColor) {
    EXPECT_EQ(opposite(Color::White), Color::Black);
    EXPECT_EQ(opposite(Color::Black), Color::White);
    EXPECT_EQ(opposite(Color::None), Color::None);

    // Operator ~ should match opposite
    EXPECT_EQ(~Color::White, Color::Black);
    EXPECT_EQ(~Color::Black, Color::White);
}

// Test pieceColor helper
TEST(ChessTypesTest, PieceColorExtraction) {
    EXPECT_EQ(pieceColor(Piece::WhitePawn), Color::White);
    EXPECT_EQ(pieceColor(Piece::WhiteKing), Color::White);
    EXPECT_EQ(pieceColor(Piece::BlackPawn), Color::Black);
    EXPECT_EQ(pieceColor(Piece::BlackKing), Color::Black);
    EXPECT_EQ(pieceColor(Piece::None), Color::None);
}

// Test pieceType helper
TEST(ChessTypesTest, PieceTypeExtraction) {
    EXPECT_EQ(pieceType(Piece::WhitePawn), PieceType::Pawn);
    EXPECT_EQ(pieceType(Piece::BlackPawn), PieceType::Pawn);
    EXPECT_EQ(pieceType(Piece::WhiteKing), PieceType::King);
    EXPECT_EQ(pieceType(Piece::BlackKing), PieceType::King);
    EXPECT_EQ(pieceType(Piece::None), PieceType::None);
}

// Test Square to String conversions and round-trips
TEST(ChessTypesTest, SquareToStringRoundTrip) {
    // Check specific conversions
    EXPECT_EQ(squareToString(Square::A1), "a1");
    EXPECT_EQ(squareToString(Square::E4), "e4");
    EXPECT_EQ(squareToString(Square::H8), "h8");
    EXPECT_EQ(squareToString(Square::None), "-");

    EXPECT_EQ(stringToSquare("a1"), Square::A1);
    EXPECT_EQ(stringToSquare("e4"), Square::E4);
    EXPECT_EQ(stringToSquare("h8"), Square::H8);
    EXPECT_EQ(stringToSquare("-"), Square::None);

    // Test round-trip for all 64 squares
    for (int i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        std::string str = squareToString(sq);
        Square parsed = stringToSquare(str);
        EXPECT_EQ(parsed, sq);
    }
}

// Test 64 square constants
TEST(ChessTypesTest, SquareConstants) {
    EXPECT_EQ(SQ_A1, Square::A1);
    EXPECT_EQ(SQ_E4, Square::E4);
    EXPECT_EQ(SQ_H8, Square::H8);
    EXPECT_EQ(SQ_NONE, Square::None);
}
