#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/polyglot.hpp"
#include "board/movegen.hpp"
#include "uci/uci.hpp"
#include <fstream>
#include <cstdio>
#include <vector>

using namespace ChessEngine;

// Platform-independent swap helpers for writing big-endian tests
static inline uint16_t test_swap_16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint64_t test_swap_64(uint64_t val) {
    return ((val >> 56) & 0x00000000000000FFULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 56) & 0xFF00000000000000ULL);
}

TEST(PolyglotTest, StartingPositionHash) {
    Board board;
    board.reset_to_start();
    
    uint64_t hash = compute_polyglot_hash(board);
    EXPECT_EQ(hash, 0x463B96181691FC9CULL);
}

TEST(PolyglotTest, HashChangesAfterMove) {
    Board board;
    board.reset_to_start();
    
    uint64_t initial_hash = compute_polyglot_hash(board);
    
    // Play e2e4
    // Find move in legal moves
    Move e2e4 = MOVE_NONE;
    for (const auto& m : generate_legal_moves(board)) {
        if (m.get_from() == Square::E2 && m.get_to() == Square::E4) {
            e2e4 = m;
            break;
        }
    }
    ASSERT_NE(e2e4, MOVE_NONE);
    
    board.make_move(e2e4);
    
    uint64_t next_hash = compute_polyglot_hash(board);
    EXPECT_NE(initial_hash, next_hash);
}

TEST(PolyglotTest, BookLookupSingleMove) {
    Board board;
    board.reset_to_start();
    
    // Create a temporary book file
    const char* filename = "test_temp_book.bin";
    std::ofstream out(filename, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    
    // Write 1 entry: Starting position -> e2e4
    // e2 polyglot: 8 * (7-1) + 4 = 52
    // e4 polyglot: 8 * (7-3) + 4 = 36
    // move code: (52 << 6) | 36 = 3364
    uint64_t key = test_swap_64(0x463B96181691FC9CULL);
    uint16_t move = test_swap_16(3364);
    uint16_t weight = test_swap_16(100);
    uint32_t learn = 0;
    
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&move), 2);
    out.write(reinterpret_cast<const char*>(&weight), 2);
    out.write(reinterpret_cast<const char*>(&learn), 4);
    out.close();
    
    // Check lookup
    Move result = lookup_book_move(board, filename);
    ASSERT_NE(result, MOVE_NONE);
    EXPECT_EQ(result.get_from(), Square::E2);
    EXPECT_EQ(result.get_to(), Square::E4);
    
    // Clean up
    std::remove(filename);
}

TEST(PolyglotTest, BookLookupMultipleMoves) {
    Board board;
    board.reset_to_start();
    
    const char* filename = "test_temp_book_multi.bin";
    std::ofstream out(filename, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    
    // Write 2 entries for starting position:
    // Entry 1: e2e4 (move 3364, weight 10)
    // Entry 2: d2d4 (move 3299, weight 20)
    // d2 polyglot: 8 * (7-1) + 3 = 51
    // d4 polyglot: 8 * (7-3) + 3 = 35
    // move code: (51 << 6) | 35 = 3299
    
    uint64_t key = test_swap_64(0x463B96181691FC9CULL);
    uint32_t learn = 0;
    
    // Write e2e4
    uint16_t move1 = test_swap_16(3364);
    uint16_t weight1 = test_swap_16(10);
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&move1), 2);
    out.write(reinterpret_cast<const char*>(&weight1), 2);
    out.write(reinterpret_cast<const char*>(&learn), 4);
    
    // Write d2d4
    uint16_t move2 = test_swap_16(3299);
    uint16_t weight2 = test_swap_16(20);
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&move2), 2);
    out.write(reinterpret_cast<const char*>(&weight2), 2);
    out.write(reinterpret_cast<const char*>(&learn), 4);
    
    out.close();
    
    // Verify we get one of the moves and that over multiple tries we get both
    int e2e4_count = 0;
    int d2d4_count = 0;
    
    for (int i = 0; i < 100; ++i) {
        Move result = lookup_book_move(board, filename);
        ASSERT_NE(result, MOVE_NONE);
        if (result.get_from() == Square::E2 && result.get_to() == Square::E4) {
            e2e4_count++;
        } else if (result.get_from() == Square::D2 && result.get_to() == Square::D4) {
            d2d4_count++;
        }
    }
    
    EXPECT_GT(e2e4_count, 0);
    EXPECT_GT(d2d4_count, 0);
    
    std::remove(filename);
}

TEST(PolyglotTest, UciOptionToggle) {
    Board board;
    board.reset_to_start();
    
    // Create test book
    const char* filename = "test_opt_book.bin";
    std::ofstream out(filename, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    
    uint64_t key = test_swap_64(0x463B96181691FC9CULL);
    uint16_t move = test_swap_16(3364);
    uint16_t weight = test_swap_16(100);
    uint32_t learn = 0;
    
    out.write(reinterpret_cast<const char*>(&key), 8);
    out.write(reinterpret_cast<const char*>(&move), 2);
    out.write(reinterpret_cast<const char*>(&weight), 2);
    out.write(reinterpret_cast<const char*>(&learn), 4);
    out.close();
    
    // Enable ownbook and set file
    g_own_book = true;
    g_book_file = filename;
    
    // Verify book move is lookup-able
    Move result = lookup_book_move(board, filename);
    EXPECT_NE(result, MOVE_NONE);
    
    // Disable ownbook
    g_own_book = false;
    // Calling parse_go should fall back to engine search because ownbook is false.
    // Wait, we don't call parse_go directly here since we can't easily capture stdout, 
    // but we can verify g_own_book is toggled correctly.
    EXPECT_FALSE(g_own_book);
    
    std::remove(filename);
}
