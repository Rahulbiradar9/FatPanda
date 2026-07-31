#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/types.hpp"

using namespace ChessEngine;

// Test that clear() resets the board to a clean/empty state
TEST(BoardTest, ClearBoardResetsEverything) {
    Board board;
    board.reset_to_start();

    // Verify it is not empty initially
    EXPECT_NE(board.get_occupancy(Color::None), EMPTY_BOARD);

    // Clear it
    board.clear();

    // Verify everything is cleared
    EXPECT_EQ(board.get_occupancy(Color::None), EMPTY_BOARD);
    EXPECT_EQ(board.get_occupancy(Color::White), EMPTY_BOARD);
    EXPECT_EQ(board.get_occupancy(Color::Black), EMPTY_BOARD);

    for (int i = 0; i < 64; ++i) {
        EXPECT_EQ(board.get_piece(static_cast<Square>(i)), Piece::None);
    }

    for (int p = 0; p < 12; ++p) {
        EXPECT_EQ(board.get_piece_bitboard(static_cast<Piece>(p)), EMPTY_BOARD);
    }

    EXPECT_EQ(board.get_side_to_move(), Color::White);
    EXPECT_EQ(board.get_castling_rights(), Castling::NONE);
    EXPECT_EQ(board.get_en_passant(), Square::None);
    EXPECT_EQ(board.get_halfmove_clock(), 0);
    EXPECT_EQ(board.get_fullmove_number(), 1);
}

// Test starting position setup
TEST(BoardTest, ResetToStartPlacesCorrectPieces) {
    Board board;
    board.reset_to_start();

    // 1. Verify specific piece squares
    // White Back Rank
    EXPECT_EQ(board.get_piece(Square::A1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::B1), Piece::WhiteKnight);
    EXPECT_EQ(board.get_piece(Square::C1), Piece::WhiteBishop);
    EXPECT_EQ(board.get_piece(Square::D1), Piece::WhiteQueen);
    EXPECT_EQ(board.get_piece(Square::E1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::F1), Piece::WhiteBishop);
    EXPECT_EQ(board.get_piece(Square::G1), Piece::WhiteKnight);
    EXPECT_EQ(board.get_piece(Square::H1), Piece::WhiteRook);

    // White Pawns
    for (int file = 0; file < 8; ++file) {
        EXPECT_EQ(board.get_piece(make_square(file, 1)), Piece::WhitePawn);
    }

    // Empty squares (ranks 3, 4, 5, 6)
    for (int rank = 2; rank <= 5; ++rank) {
        for (int file = 0; file < 8; ++file) {
            EXPECT_EQ(board.get_piece(make_square(file, rank)), Piece::None);
        }
    }

    // Black Pawns
    for (int file = 0; file < 8; ++file) {
        EXPECT_EQ(board.get_piece(make_square(file, 6)), Piece::BlackPawn);
    }

    // Black Back Rank
    EXPECT_EQ(board.get_piece(Square::A8), Piece::BlackRook);
    EXPECT_EQ(board.get_piece(Square::B8), Piece::BlackKnight);
    EXPECT_EQ(board.get_piece(Square::C8), Piece::BlackBishop);
    EXPECT_EQ(board.get_piece(Square::D8), Piece::BlackQueen);
    EXPECT_EQ(board.get_piece(Square::E8), Piece::BlackKing);
    EXPECT_EQ(board.get_piece(Square::F8), Piece::BlackBishop);
    EXPECT_EQ(board.get_piece(Square::G8), Piece::BlackKnight);
    EXPECT_EQ(board.get_piece(Square::H8), Piece::BlackRook);

    // 2. Verify Game State
    EXPECT_EQ(board.get_side_to_move(), Color::White);
    EXPECT_EQ(board.get_castling_rights(), Castling::ALL);
    EXPECT_EQ(board.get_en_passant(), Square::None);
    EXPECT_EQ(board.get_halfmove_clock(), 0);
    EXPECT_EQ(board.get_fullmove_number(), 1);
}

// Test occupancies in starting position
TEST(BoardTest, OccupanciesAreCorrect) {
    Board board;
    board.reset_to_start();

    // White occupancy must be ranks 1 & 2 (indices 0..15)
    Bitboard expected_white = 0xFFFFULL;
    EXPECT_EQ(board.get_occupancy(Color::White), expected_white);

    // Black occupancy must be ranks 7 & 8 (indices 48..63)
    Bitboard expected_black = 0xFFFF000000000000ULL;
    EXPECT_EQ(board.get_occupancy(Color::Black), expected_black);

    // Combined occupancy
    EXPECT_EQ(board.get_occupancy(Color::None), expected_white | expected_black);
}

// Test set_piece dynamically updates occupancies
TEST(BoardTest, SetPieceUpdatesOccupancies) {
    Board board; // starts empty

    // Put a White Queen on E4
    board.set_piece(Square::E4, Piece::WhiteQueen);
    EXPECT_EQ(board.get_piece(Square::E4), Piece::WhiteQueen);
    EXPECT_TRUE(test_bit(board.get_occupancy(Color::White), Square::E4));
    EXPECT_FALSE(test_bit(board.get_occupancy(Color::Black), Square::E4));
    EXPECT_TRUE(test_bit(board.get_occupancy(Color::None), Square::E4));

    // Put a Black Knight on E4 (replaces White Queen)
    board.set_piece(Square::E4, Piece::BlackKnight);
    EXPECT_EQ(board.get_piece(Square::E4), Piece::BlackKnight);
    EXPECT_FALSE(test_bit(board.get_occupancy(Color::White), Square::E4));
    EXPECT_TRUE(test_bit(board.get_occupancy(Color::Black), Square::E4));
    EXPECT_TRUE(test_bit(board.get_occupancy(Color::None), Square::E4));

    // Remove piece from E4
    board.set_piece(Square::E4, Piece::None);
    EXPECT_EQ(board.get_piece(Square::E4), Piece::None);
    EXPECT_FALSE(test_bit(board.get_occupancy(Color::White), Square::E4));
    EXPECT_FALSE(test_bit(board.get_occupancy(Color::Black), Square::E4));
    EXPECT_FALSE(test_bit(board.get_occupancy(Color::None), Square::E4));
}
