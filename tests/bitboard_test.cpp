#include <gtest/gtest.h>
#include "board/bitboard.hpp"
#include "board/types.hpp"

using namespace ChessEngine;

// Test set_bit, clear_bit, and test_bit
TEST(BitboardTest, SetClearTestOperations) {
    Bitboard bb = EMPTY_BOARD;
    EXPECT_EQ(bb, 0ULL);

    // Test initial state
    EXPECT_FALSE(test_bit(bb, Square::A1));
    EXPECT_FALSE(test_bit(bb, Square::H8));

    // Set A1
    set_bit(bb, Square::A1);
    EXPECT_TRUE(test_bit(bb, Square::A1));
    EXPECT_EQ(bb, 1ULL);

    // Set H8
    set_bit(bb, Square::H8);
    EXPECT_TRUE(test_bit(bb, Square::H8));
    EXPECT_EQ(bb, 1ULL | (1ULL << 63));

    // Clear A1
    clear_bit(bb, Square::A1);
    EXPECT_FALSE(test_bit(bb, Square::A1));
    EXPECT_TRUE(test_bit(bb, Square::H8));
    EXPECT_EQ(bb, (1ULL << 63));

    // Clear H8
    clear_bit(bb, Square::H8);
    EXPECT_FALSE(test_bit(bb, Square::H8));
    EXPECT_EQ(bb, EMPTY_BOARD);
}

// Test bit counting (popcount)
TEST(BitboardTest, CountBitsOperation) {
    Bitboard bb = EMPTY_BOARD;
    EXPECT_EQ(count_bits(bb), 0);

    set_bit(bb, Square::A1);
    EXPECT_EQ(count_bits(bb), 1);

    set_bit(bb, Square::E4);
    set_bit(bb, Square::H8);
    EXPECT_EQ(count_bits(bb), 3);

    bb = FULL_BOARD;
    EXPECT_EQ(count_bits(bb), 64);
}

// Test getting and popping the least significant bit (LSB)
TEST(BitboardTest, GetAndPopLSB) {
    Bitboard bb = EMPTY_BOARD;

    // Test empty bitboard
    EXPECT_EQ(get_lsb(bb), Square::None);
    EXPECT_EQ(pop_lsb(bb), Square::None);

    // Populate some bits
    set_bit(bb, Square::D4);
    set_bit(bb, Square::E5);
    set_bit(bb, Square::H1);

    // LSB should be H1 (index 7)
    EXPECT_EQ(get_lsb(bb), Square::H1);
    EXPECT_EQ(pop_lsb(bb), Square::H1);

    // Next LSB should be D4 (index 27)
    EXPECT_EQ(get_lsb(bb), Square::D4);
    EXPECT_EQ(pop_lsb(bb), Square::D4);

    // Last LSB should be E5 (index 36)
    EXPECT_EQ(get_lsb(bb), Square::E5);
    EXPECT_EQ(pop_lsb(bb), Square::E5);

    // Now empty
    EXPECT_EQ(bb, EMPTY_BOARD);
    EXPECT_EQ(get_lsb(bb), Square::None);
    EXPECT_EQ(pop_lsb(bb), Square::None);
}

// Verify constexpr compile-time operations
TEST(BitboardTest, ConstexprVerification) {
    constexpr auto get_compile_time_bitboard = []() {
        Bitboard bb = EMPTY_BOARD;
        set_bit(bb, Square::C3);
        set_bit(bb, Square::F6);
        return bb;
    };

    constexpr Bitboard static_bb = get_compile_time_bitboard();
    
    // Check values at compile time
    static_assert(test_bit(static_bb, Square::C3), "C3 must be set");
    static_assert(test_bit(static_bb, Square::F6), "F6 must be set");
    static_assert(!test_bit(static_bb, Square::A1), "A1 must not be set");
    static_assert(count_bits(static_bb) == 2, "Popcount must be 2");

    // GTest runtime checks just to verify
    EXPECT_TRUE(test_bit(static_bb, Square::C3));
    EXPECT_TRUE(test_bit(static_bb, Square::F6));
    EXPECT_EQ(count_bits(static_bb), 2);
}

// Test foundational chess types helpers
TEST(ChessTypesTest, PieceHelpers) {
    Piece white_rook = make_piece(Color::White, PieceType::Rook);
    EXPECT_EQ(white_rook, Piece::WhiteRook);
    EXPECT_EQ(get_piece_type(white_rook), PieceType::Rook);
    EXPECT_EQ(get_piece_color(white_rook), Color::White);

    Piece black_queen = make_piece(Color::Black, PieceType::Queen);
    EXPECT_EQ(black_queen, Piece::BlackQueen);
    EXPECT_EQ(get_piece_type(black_queen), PieceType::Queen);
    EXPECT_EQ(get_piece_color(black_queen), Color::Black);

    EXPECT_EQ(make_piece(Color::None, PieceType::Pawn), Piece::None);
}

TEST(ChessTypesTest, SquareHelpers) {
    Square sq = make_square(2, 4); // file 2 (C), rank 4 (5) -> C5 (index 34)
    EXPECT_EQ(sq, Square::C5);
    EXPECT_EQ(get_file(sq), 2);
    EXPECT_EQ(get_rank(sq), 4);

    EXPECT_EQ(make_square(8, 0), Square::None);
    EXPECT_TRUE(is_valid_square(Square::A1));
    EXPECT_FALSE(is_valid_square(Square::None));
}
