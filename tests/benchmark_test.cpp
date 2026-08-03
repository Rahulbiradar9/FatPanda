#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/perft.hpp"
#include "search/search.hpp"
#include "hash/tt.hpp"
#include <chrono>
#include <iostream>

using namespace ChessEngine;

TEST(BenchmarkTest, PerftPerformanceNps) {
    Board board;
    board.reset_to_start();

    int depth = 5;
    auto start = std::chrono::steady_clock::now();
    auto result = runPerftBenchmark(board, depth);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double nps = 0;
    if (duration > 0) {
        nps = (static_cast<double>(result.nodes) / static_cast<double>(duration)) * 1000.0;
    }

    std::cout << "\n========================================\n"
              << "          PERFT NPS BENCHMARK           \n"
              << "========================================\n"
              << "Depth: " << depth << "\n"
              << "Nodes: " << result.nodes << "\n"
              << "Time : " << duration << " ms\n"
              << "NPS  : " << std::fixed << std::setprecision(0) << nps << " Nodes/sec\n"
              << "========================================\n\n";

    // Expect a reasonable minimum speed on the CPU (e.g. 500,000 NPS)
    EXPECT_GT(result.nodes, 0);
    if (duration > 0) {
        EXPECT_GT(nps, 500000.0);
    }
}

TEST(BenchmarkTest, SearchPerformanceNps) {
    Board board;
    board.reset_to_start();

    // Use a complex position like Kiwipete to get realistic search patterns
    ASSERT_TRUE(board.loadFromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    g_tt.clear();

    int depth = 5;
    auto start = std::chrono::steady_clock::now();
    SearchResult result = search(board, depth);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double nps = 0;
    if (duration > 0) {
        nps = (static_cast<double>(result.nodes_searched) / static_cast<double>(duration)) * 1000.0;
    }

    std::cout << "\n========================================\n"
              << "          SEARCH NPS BENCHMARK          \n"
              << "========================================\n"
              << "Depth: " << depth << "\n"
              << "Nodes: " << result.nodes_searched << "\n"
              << "Time : " << duration << " ms\n"
              << "NPS  : " << std::fixed << std::setprecision(0) << nps << " Nodes/sec\n"
              << "========================================\n\n";

    EXPECT_GT(result.nodes_searched, 0);
    if (duration > 0) {
        EXPECT_GT(nps, 40000.0); // Expect at least 40k NPS in search
    }
}

TEST(BenchmarkTest, SearchOptimizationsIndependentBenchmark) {
    Board board;
    ASSERT_TRUE(board.loadFromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));

    struct BenchmarkRun {
        std::string name;
        SearchSettings settings;
    };

    // Define runs where we disable one optimization at a time
    SearchSettings base_settings; // all true by default
    
    SearchSettings no_pvs = base_settings; no_pvs.pvs = false;
    SearchSettings no_nmp = base_settings; no_nmp.nmp = false;
    SearchSettings no_lmr = base_settings; no_lmr.lmr = false;
    SearchSettings no_asp = base_settings; no_asp.aspiration = false;
    SearchSettings no_fut = base_settings; no_fut.futility = false;
    SearchSettings no_rfp = base_settings; no_rfp.rfp = false;
    SearchSettings no_see = base_settings; no_see.see = false;

    std::vector<BenchmarkRun> runs = {
        {"All Enabled (Baseline)", base_settings},
        {"Without PVS", no_pvs},
        {"Without Null Move Pruning", no_nmp},
        {"Without Late Move Reductions", no_lmr},
        {"Without Aspiration Windows", no_asp},
        {"Without Futility Pruning", no_fut},
        {"Without Reverse Futility Pruning", no_rfp},
        {"Without Static Exchange Evaluation", no_see}
    };

    std::cout << "\n=========================================================================\n"
              << "              INDEPENDENT OPTIMIZATION BENCHMARK REPORT                 \n"
              << "=========================================================================\n"
              << std::left << std::setw(36) << "Configuration" 
              << " | " << std::setw(12) << "Nodes" 
              << " | " << std::setw(10) << "Time (ms)" 
              << " | " << std::setw(10) << "NPS" << "\n"
              << "-------------------------------------------------------------------------\n";

    uint64_t baseline_nodes = 0;

    for (auto& run : runs) {
        // Apply settings
        g_search_settings = run.settings;
        g_tt.clear();

        auto start = std::chrono::steady_clock::now();
        SearchResult result = search(board, 5); // search at depth 5
        auto end = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double nps = 0;
        if (duration > 0) {
            nps = (static_cast<double>(result.nodes_searched) / static_cast<double>(duration)) * 1000.0;
        }

        std::cout << std::left << std::setw(36) << run.name 
                  << " | " << std::setw(12) << result.nodes_searched 
                  << " | " << std::setw(10) << duration 
                  << " | " << std::fixed << std::setprecision(0) << std::setw(10) << nps << "\n";

        if (run.name == "All Enabled (Baseline)") {
            baseline_nodes = result.nodes_searched;
        } else {
            // Verify that disabling optimizations generally increases node count
            if (run.name == "Without PVS" || run.name == "Without Null Move Pruning" || run.name == "Without Late Move Reductions") {
                EXPECT_GE(result.nodes_searched, baseline_nodes);
            }
        }
    }

    std::cout << "=========================================================================\n\n";

    // Restore settings
    g_search_settings = base_settings;
}

