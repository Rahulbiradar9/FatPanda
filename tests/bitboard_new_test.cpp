#include <gtest/gtest.h>
#include "board/bitboard.hpp"

using namespace ChessEngine;

// Test setBit, clearBit, getBit, and toggleBit operations (both Square and int overloads)
TEST(BitboardNewTest, SetClearGetToggleOperations) {
    Bitboard bb = EMPTY_BOARD;
    EXPECT_EQ(bb, 0ULL);

    // Initial check
    EXPECT_FALSE(getBit(bb, Square::A1));
    EXPECT_FALSE(getBit(bb, 0));
    EXPECT_FALSE(getBit(bb, Square::H8));
    EXPECT_FALSE(getBit(bb, 63));

    // setBit with Square
    setBit(bb, Square::A1);
    EXPECT_TRUE(getBit(bb, Square::A1));
    EXPECT_TRUE(getBit(bb, 0));
    EXPECT_EQ(bb, 1ULL);

    // setBit with int
    setBit(bb, 63);
    EXPECT_TRUE(getBit(bb, Square::H8));
    EXPECT_TRUE(getBit(bb, 63));
    EXPECT_EQ(bb, 1ULL | (1ULL << 63));

    // toggleBit with Square
    toggleBit(bb, Square::A1);
    EXPECT_FALSE(getBit(bb, Square::A1));
    EXPECT_TRUE(getBit(bb, Square::H8));

    // toggleBit with int
    toggleBit(bb, 63);
    EXPECT_FALSE(getBit(bb, Square::H8));
    EXPECT_EQ(bb, EMPTY_BOARD);

    // toggleBit to set and clear
    toggleBit(bb, 10);
    EXPECT_TRUE(getBit(bb, 10));
    toggleBit(bb, 10);
    EXPECT_FALSE(getBit(bb, 10));

    // clearBit with Square
    setBit(bb, Square::E4);
    EXPECT_TRUE(getBit(bb, Square::E4));
    clearBit(bb, Square::E4);
    EXPECT_FALSE(getBit(bb, Square::E4));

    // clearBit with int
    setBit(bb, 42);
    EXPECT_TRUE(getBit(bb, 42));
    clearBit(bb, 42);
    EXPECT_FALSE(getBit(bb, 42));
}

// Test popLSB, lsb, and msb operations
TEST(BitboardNewTest, LsbMsbAndPopOperations) {
    Bitboard bb = EMPTY_BOARD;

    // Empty board checks
    EXPECT_EQ(lsb(bb), Square::None);
    EXPECT_EQ(msb(bb), Square::None);
    EXPECT_EQ(popLSB(bb), Square::None);

    // Single bit checks
    setBit(bb, 7); // H1
    EXPECT_EQ(lsb(bb), Square::H1);
    EXPECT_EQ(msb(bb), Square::H1);

    // Multi bit checks
    setBit(bb, 36); // E5
    setBit(bb, 63); // H8
    
    // Bits are now at 7, 36, 63
    EXPECT_EQ(lsb(bb), Square::H1);
    EXPECT_EQ(msb(bb), Square::H8);

    // popLSB round-trip
    EXPECT_EQ(popLSB(bb), Square::H1);
    EXPECT_EQ(lsb(bb), Square::E5);
    EXPECT_EQ(msb(bb), Square::H8);

    EXPECT_EQ(popLSB(bb), Square::E5);
    EXPECT_EQ(lsb(bb), Square::H8);
    EXPECT_EQ(msb(bb), Square::H8);

    EXPECT_EQ(popLSB(bb), Square::H8);
    EXPECT_EQ(bb, EMPTY_BOARD);
    EXPECT_EQ(lsb(bb), Square::None);
    EXPECT_EQ(msb(bb), Square::None);
}

// Test countBits
TEST(BitboardNewTest, CountBits) {
    Bitboard bb = EMPTY_BOARD;
    EXPECT_EQ(countBits(bb), 0);

    setBit(bb, 0);
    EXPECT_EQ(countBits(bb), 1);

    setBit(bb, 10);
    setBit(bb, 20);
    EXPECT_EQ(countBits(bb), 3);

    bb = FULL_BOARD;
    EXPECT_EQ(countBits(bb), 64);
}

// Verify constexpr compilation for the new helpers
TEST(BitboardNewTest, ConstexprVerification) {
    constexpr auto get_compile_time_bb = []() {
        Bitboard bb = EMPTY_BOARD;
        setBit(bb, Square::D4);
        setBit(bb, 56); // A8
        return bb;
    };

    constexpr Bitboard static_bb = get_compile_time_bb();

    static_assert(getBit(static_bb, Square::D4), "D4 must be set");
    static_assert(getBit(static_bb, 56), "A8 must be set");
    static_assert(!getBit(static_bb, Square::A1), "A1 must not be set");
    static_assert(countBits(static_bb) == 2, "Count must be 2");
    static_assert(lsb(static_bb) == Square::D4, "LSB must be D4");
    static_assert(msb(static_bb) == Square::A8, "MSB must be A8");
}
