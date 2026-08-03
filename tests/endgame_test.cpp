#include <gtest/gtest.h>
#include "board/board.hpp"
#include "evaluation/evaluation.hpp"

using namespace ChessEngine;

// Forward declare internal evaluation helpers to test them
namespace ChessEngine {
    int get_game_phase(const Board& board);
    int evaluateEndgameHeuristics(const Board& board);
}

TEST(EndgameTest, InsufficientMaterial) {
    Board board;

    // 1. King vs King
    ASSERT_TRUE(board.loadFromFen("8/8/8/8/8/8/8/k6K w - - 0 1"));
    EXPECT_TRUE(board.is_insufficient_material());

    // 2. King + Bishop vs King
    ASSERT_TRUE(board.loadFromFen("8/8/8/8/8/8/kb6/7K w - - 0 1"));
    EXPECT_TRUE(board.is_insufficient_material());

    // 3. King + Knight vs King
    ASSERT_TRUE(board.loadFromFen("8/8/8/8/8/8/kn6/7K w - - 0 1"));
    EXPECT_TRUE(board.is_insufficient_material());

    // 4. King + Bishop vs King + Bishop on same color square
    // Black bishop on a1 (light), White bishop on c1 (light)
    ASSERT_TRUE(board.loadFromFen("7k/8/8/8/8/8/8/b1B4K w - - 0 1"));
    EXPECT_TRUE(board.is_insufficient_material());

    // 5. King + Bishop vs King + Bishop on opposite color square
    // Black bishop on a1 (light), White bishop on b1 (dark)
    ASSERT_TRUE(board.loadFromFen("7k/8/8/8/8/8/8/bB5K w - - 0 1"));
    EXPECT_FALSE(board.is_insufficient_material());

    // 6. King + Pawn vs King (pawns can promote, so not insufficient material)
    ASSERT_TRUE(board.loadFromFen("7k/8/8/8/8/8/P7/7K w - - 0 1"));
    EXPECT_FALSE(board.is_insufficient_material());
}

TEST(EndgameTest, EvaluationInterpolation) {
    Board board;

    // 1. Start position (lots of non-pawn material)
    board.reset_to_start();
    int start_npm = get_game_phase(board);
    EXPECT_GT(start_npm, 4000); // Max is ~8200

    // 2. Endgame position: King + Queen vs King
    ASSERT_TRUE(board.loadFromFen("k7/8/8/8/8/8/8/Q6K w - - 0 1"));
    int eg_npm = get_game_phase(board);
    EXPECT_EQ(eg_npm, 900); // Just one White Queen (900)
}

TEST(EndgameTest, MopUpHeuristics) {
    Board board;

    // King + Rook vs King (White Rook and King vs Black King)
    // Position A: Black King in center (d4)
    ASSERT_TRUE(board.loadFromFen("8/8/8/8/3k4/8/8/R6K w - - 0 1"));
    int score_center = evaluateEndgameHeuristics(board);

    // Position B: Black King in corner (a8)
    ASSERT_TRUE(board.loadFromFen("k7/8/8/8/8/8/8/R6K w - - 0 1"));
    int score_corner = evaluateEndgameHeuristics(board);

    // Score driving the lone King to the corner should be higher
    EXPECT_GT(score_corner, score_center);
}
