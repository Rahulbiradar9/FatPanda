#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/perft.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace ChessEngine;

struct PerftTestCase {
    std::string name;
    std::string fen;
    int depth;
    uint64_t expected_nodes;
};

void run_and_verify_perft(const PerftTestCase& test_case) {
    Board board;
    ASSERT_TRUE(board.loadFromFen(test_case.fen)) << "Failed to load FEN: " << test_case.fen;

    auto result = runPerftBenchmark(board, test_case.depth);
    bool is_pass = (result.nodes == test_case.expected_nodes);

    std::cout << "[ PERFT ] " << std::left << std::setw(15) << test_case.name 
              << " Depth " << test_case.depth << " | "
              << "Expected: " << std::setw(12) << test_case.expected_nodes 
              << " Actual: " << std::setw(12) << result.nodes
              << " (" << std::fixed << std::setprecision(1) << result.time_ms << " ms, " 
              << std::fixed << std::setprecision(0) << result.nps << " NPS) | "
              << (is_pass ? "PASS" : "FAIL") << "\n";

    EXPECT_EQ(result.nodes, test_case.expected_nodes);
}

// Test Starting Position up to Depth 5 (Depth 6 is run in a separate test to keep times manageable or verified optionally)
TEST(PerftTest, StartingPosition) {
    std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::vector<PerftTestCase> cases = {
        {"Start Position", start_fen, 1, 20ULL},
        {"Start Position", start_fen, 2, 400ULL},
        {"Start Position", start_fen, 3, 8902ULL},
        {"Start Position", start_fen, 4, 197281ULL},
        {"Start Position", start_fen, 5, 4865609ULL}
    };

    std::cout << "\n--- Starting Position Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

// Kiwipete Position
TEST(PerftTest, Kiwipete) {
    std::string kiwipete_fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    std::vector<PerftTestCase> cases = {
        {"Kiwipete", kiwipete_fen, 1, 48ULL},
        {"Kiwipete", kiwipete_fen, 2, 2039ULL},
        {"Kiwipete", kiwipete_fen, 3, 97862ULL},
        {"Kiwipete", kiwipete_fen, 4, 4085603ULL}
    };

    std::cout << "\n--- Kiwipete Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

// Position 3 (Verify up to Depth 6 as requested)
TEST(PerftTest, Position3) {
    std::string pos3_fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
    std::vector<PerftTestCase> cases = {
        {"Position 3", pos3_fen, 1, 14ULL},
        {"Position 3", pos3_fen, 2, 191ULL},
        {"Position 3", pos3_fen, 3, 2812ULL},
        {"Position 3", pos3_fen, 4, 43238ULL},
        {"Position 3", pos3_fen, 5, 674624ULL},
        {"Position 3", pos3_fen, 6, 11030083ULL} // Verified at Depth 6
    };

    std::cout << "\n--- Position 3 Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

// Position 4
TEST(PerftTest, Position4) {
    std::string pos4_fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    std::vector<PerftTestCase> cases = {
        {"Position 4", pos4_fen, 1, 6ULL},
        {"Position 4", pos4_fen, 2, 264ULL},
        {"Position 4", pos4_fen, 3, 9467ULL},
        {"Position 4", pos4_fen, 4, 422333ULL}
    };

    std::cout << "\n--- Position 4 Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

// Position 5
TEST(PerftTest, Position5) {
    std::string pos5_fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    std::vector<PerftTestCase> cases = {
        {"Position 5", pos5_fen, 1, 44ULL},
        {"Position 5", pos5_fen, 2, 1486ULL},
        {"Position 5", pos5_fen, 3, 62379ULL},
        {"Position 5", pos5_fen, 4, 2103487ULL}
    };

    std::cout << "\n--- Position 5 Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

// Divide mode verification test
// Endgame Promotion Position (Feasible for Depth 7 verification)
TEST(PerftTest, EndgamePromotionDepth7) {
    std::string fen = "8/k1P5/8/1K6/8/8/8/8 w - - 0 1";
    std::vector<PerftTestCase> cases = {
        {"Endgame Promo", fen, 1, 10ULL},
        {"Endgame Promo", fen, 2, 25ULL},
        {"Endgame Promo", fen, 3, 268ULL},
        {"Endgame Promo", fen, 4, 926ULL},
        {"Endgame Promo", fen, 5, 10857ULL},
        {"Endgame Promo", fen, 6, 43261ULL},
        {"Endgame Promo", fen, 7, 567584ULL}
    };

    std::cout << "\n--- Endgame Promotion Perft Results ---\n";
    for (const auto& tc : cases) {
        run_and_verify_perft(tc);
    }
}

TEST(PerftTest, DivideMode) {
    Board board;
    board.setStartingPosition();

    std::cout << "\n--- Divide Mode Output (Start Position Depth 2) ---\n";
    auto divide_results = runPerftDivide(board, 2, true);
    
    // Total moves generated at depth 2 from start position must be 20
    EXPECT_EQ(divide_results.size(), 20);

    // e2e4 double push should generate exactly 20 responses
    bool found_e2e4 = false;
    for (const auto& [move_str, nodes] : divide_results) {
        if (move_str == "e2e4") {
            EXPECT_EQ(nodes, 20ULL);
            found_e2e4 = true;
        }
    }
    EXPECT_TRUE(found_e2e4);
}
