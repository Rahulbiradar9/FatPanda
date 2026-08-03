#include <gtest/gtest.h>
#include "board/board.hpp"
#include "search/search.hpp"
#include "hash/tt.hpp"

using namespace ChessEngine;

// Test that search runs and respects the depth parameter, and tracks nodes
TEST(SearchTest, RespectsDepthAndTracksNodes) {
    Board board;
    board.reset_to_start();

    // Search at depth 1
    SearchResult result1 = search(board, 1);
    EXPECT_FALSE(result1.best_move.is_none());
    EXPECT_NE(result1.score, 0);

    // Search at depth 2
    SearchResult result2 = search(board, 2);
    EXPECT_FALSE(result2.best_move.is_none());
}

// Test that the search finds a Mate in 1
TEST(SearchTest, FindsMateIn1) {
    Board board;
    // FEN: Black king in corner, White rook can checkmate on g8
    const std::string mate_in_1_fen = "k7/6R1/1K6/8/8/8/8/8 w - - 0 1";
    ASSERT_TRUE(board.load_from_fen(mate_in_1_fen));

    SearchResult result = search(board, 2);
    EXPECT_EQ(result.best_move.to_string(), "g7g8");
    EXPECT_GT(result.score, MATE_SCORE - 10);
}

// Test that the search finds a Mate in 2
TEST(SearchTest, FindsMateIn2) {
    Board board;
    // FEN: White king on c6, White rook on g7, Black king on a8.
    // White to move: Kb6 (c6b6) forces Kb8 (a8b8), then Rg8# (g7g8) is mate.
    const std::string mate_in_2_fen = "k7/6R1/2K5/8/8/8/8/8 w - - 0 1";
    ASSERT_TRUE(board.load_from_fen(mate_in_2_fen));

    // Must search at least depth 3 to find a mate in 2 (3 plies: 1. Kb6 Kb8 2. Rg8#)
    SearchResult result = search(board, 3);
    EXPECT_EQ(result.best_move.to_string(), "c6b6");
    EXPECT_GT(result.score, MATE_SCORE - 10);
}

// Test stalemate detection
TEST(SearchTest, HandlesStalemateCorrectly) {
    Board board;
    // FEN: Black is stalemated
    const std::string stalemate_fen = "k7/P7/1K6/8/8/8/8/8 b - - 0 1";
    ASSERT_TRUE(board.load_from_fen(stalemate_fen));

    // Depth 1 search at stalemate position
    SearchResult result = search(board, 1);
    EXPECT_TRUE(result.best_move.is_none());
    EXPECT_EQ(result.score, 0);
}

// Test that 50-move rule draw is scored as 0
TEST(SearchTest, Handles50MoveDraw) {
    Board board;
    board.reset_to_start();
    board.set_halfmove_clock(100); // 50 full moves without pawn push or capture

    SearchResult result = search(board, 1);
    EXPECT_EQ(result.score, 0);
}

// Test that search returns nodes, depth, and a non-empty PV line
TEST(SearchTest, ReturnsNodesDepthAndPv) {
    Board board;
    board.reset_to_start();

    SearchResult result = search(board, 2);
    EXPECT_GT(result.nodes_searched, 0);
    EXPECT_EQ(result.search_depth, 2);
    EXPECT_FALSE(result.pv.empty());
    
    // The first move in the PV should be the best move
    EXPECT_EQ(result.pv[0], result.best_move);
}

// Test that verifies move ordering is efficient by comparing search nodes
TEST(SearchTest, MoveOrderingBenchmark) {
    Board board;
    // Kiwipete position (rich tactical position, great for testing move ordering cutoffs)
    ASSERT_TRUE(board.load_from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));

    g_tt.clear();
    SearchResult result = search(board, 4);
    
    // The search at depth 4 in Kiwipete should be extremely fast and visit relatively few nodes
    // thanks to Alpha-Beta, TT, MVV-LVA, Killer moves, and History heuristics.
    std::cout << "[Benchmark] Kiwipete depth 4 nodes: " << result.nodes_searched << std::endl;
    EXPECT_GT(result.nodes_searched, 0);
    EXPECT_LT(result.nodes_searched, 150000); // Usually < 80,000 nodes with good move ordering
}

// Test that search respects g_stop_search flag and returns early
TEST(SearchTest, SearchRespectsStopFlag) {
    Board board;
    board.reset_to_start();

    // Start search and set stop flag immediately
    g_stop_search.store(true);
    SearchResult result = search(board, 10);
    
    // It should exit immediately and return early
    EXPECT_LT(result.nodes_searched, 5000);
    
    // Reset stop search flag
    g_stop_search.store(false);
}

// Test that search identifies threefold repetition as a draw (0 score)
TEST(SearchTest, DetectsThreefoldRepetitionDraw) {
    Board board;
    // Load a position where White is down a Queen (RNB1KBNR on back rank)
    ASSERT_TRUE(board.loadFromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"));

    // Force a repetition sequence in the board history:
    // 1. Nf3 Nf6 2. Ng1 Ng8 3. Nf3 Nf6
    Move nf3(Square::G1, Square::F3, MoveFlag::NORMAL);
    Move nf6(Square::G8, Square::F6, MoveFlag::NORMAL);
    Move ng1(Square::F3, Square::G1, MoveFlag::NORMAL);
    Move ng8(Square::F6, Square::G8, MoveFlag::NORMAL);

    UndoState u1, u2, u3, u4, u5, u6;
    ASSERT_TRUE(board.makeMove(nf3, u1));
    ASSERT_TRUE(board.makeMove(nf6, u2));
    ASSERT_TRUE(board.makeMove(ng1, u3));
    ASSERT_TRUE(board.makeMove(ng8, u4));
    ASSERT_TRUE(board.makeMove(nf3, u5));
    ASSERT_TRUE(board.makeMove(nf6, u6));

    // The current position occurred twice.
    // If White plays Ng1, it will be the 3rd occurrence (draw).
    // The search at depth 2 should recognize this and evaluate it as 0.
    SearchResult result = search(board, 2);
    
    // Best move should be ng1 (f3g1) since it claims a draw (0 score),
    // which is much better than playing a normal move while down a Queen (-900+ score).
    EXPECT_EQ(result.best_move.to_string(), "f3g1");
    EXPECT_EQ(result.score, 0);
}

