#include <gtest/gtest.h>
#include "board/board.hpp"
#include "search/search.hpp"
#include "hash/tt.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace ChessEngine;

TEST(LazySmpTest, ConfigurableThreads) {
    Board board;
    board.reset_to_start();

    // Verify search works cleanly for different thread counts without deadlock
    for (int threads : {1, 2, 4}) {
        g_num_threads = threads;
        g_stop_search = false;
        
        SearchResult result = search(board, 3);
        EXPECT_NE(result.best_move, MOVE_NONE);
        EXPECT_GT(result.nodes_searched, 0);
    }
    
    // Reset to default
    g_num_threads = 1;
}

TEST(LazySmpTest, BenchmarkScaling) {
    Board board;
    // Set up a complex middlegame position with lots of pieces for better thread scaling
    // FEN from a tactical middlegame
    ASSERT_TRUE(board.loadFromFen("r1b2rk1/pp1p1ppp/2n1pn2/q1p3B1/2PP4/2PBPN2/P4PPP/R2QK2R w KQ - 3 9"));

    std::cout << "\n=========================================================================\n";
    std::cout << "                 LAZY SMP MULTITHREADED SCALING BENCHMARK                \n";
    std::cout << "=========================================================================\n";
    std::cout << std::left << std::setw(12) << "Threads"
              << std::setw(15) << "Nodes"
              << std::setw(15) << "Time (ms)"
              << std::setw(15) << "NPS" << "\n";
    std::cout << "-------------------------------------------------------------------------\n";

    // Benchmark depth
    const int target_depth = 5;

    for (int threads : {1, 2, 4, 8, 16}) {
        g_num_threads = threads;
        g_tt.clear();
        g_stop_search = false;
        
        // Track time limits
        g_time_limit_soft_ms = -1;
        g_time_limit_hard_ms = -1;
        
        auto start = std::chrono::steady_clock::now();
        SearchResult result = search(board, target_depth);
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        uint64_t nps = duration > 0 ? (result.nodes_searched * 1000) / duration : result.nodes_searched;

        std::cout << std::left << std::setw(12) << threads
                  << std::setw(15) << result.nodes_searched
                  << std::setw(15) << duration
                  << std::setw(15) << nps << "\n";
    }
    std::cout << "=========================================================================\n\n";

    // Reset to default
    g_num_threads = 1;
}
