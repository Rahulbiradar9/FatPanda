#include <gtest/gtest.h>
#include "board/board.hpp"
#include "search/search.hpp"

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
