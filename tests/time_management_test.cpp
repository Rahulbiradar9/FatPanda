#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/movegen.hpp"
#include "search/search.hpp"
#include "uci/uci.hpp"
#include <sstream>
#include <chrono>
#include <thread>

using namespace ChessEngine;

TEST(TimeManagementTest, ParsesMovetimeCorrectly) {
    Board board;
    board.reset_to_start();
    std::stringstream ss("movetime 500");
    
    g_search_overhead_ms = 25;
    g_stop_search.store(true); // Stop background search thread immediately
    
    parse_go(board, ss);
    
    EXPECT_EQ(g_time_limit_soft_ms, 500 - 25);
    EXPECT_EQ(g_time_limit_hard_ms, 500 - 25);
    
    join_search_thread();
}

TEST(TimeManagementTest, RemainingClockAllocation) {
    Board board;
    board.reset_to_start(); // White to move
    std::stringstream ss("wtime 10000 winc 1000 movestogo 20");
    
    g_search_overhead_ms = 20;
    g_stop_search.store(true);
    
    parse_go(board, ss);
    
    // soft_limit = (10000 / 22) + 800 - 20 = 454 + 800 - 20 = 1234 ms
    EXPECT_EQ(g_time_limit_soft_ms, 1234);
    // hard_limit = min(10000 - 20, 5000 + 1000 - 20) = 5980 ms
    EXPECT_EQ(g_time_limit_hard_ms, 5980);
    
    join_search_thread();
}

TEST(TimeManagementTest, ForcedMoveSearchDepth) {
    Board board;
    g_stop_search.store(false); // Reset stop flag for this direct search call
    // FEN position where White is in check and has only one escape square:
    // White king at h1 is checked by rook at h2. White rook at g1 blocks King escape to g1.
    ASSERT_TRUE(board.loadFromFen("7k/8/8/8/8/8/7r/6RK w - - 0 1"));
    
    // Verify only one legal move
    std::vector<Move> legal = generate_legal_moves(board);
    ASSERT_EQ(legal.size(), 1);
    
    // Search should stop after depth 1
    SearchResult result = search(board, 10);
    EXPECT_EQ(result.search_depth, 1);
}
