#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/types.hpp"
#include "board/movegen.hpp"
#include "evaluation/evaluation.hpp"

using namespace ChessEngine;

// Helper to evaluate with checkmate/stalemate detection
int get_game_state_score(Board& board) {
    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (is_in_check(board, board.get_side_to_move())) {
            // Checkmate! If White is in checkmate, Black wins (-30000). If Black is in checkmate, White wins (+30000).
            return (board.get_side_to_move() == Color::White) ? -30000 : 30000;
        } else {
            // Stalemate (draw)
            return 0;
        }
    }
    return evaluate(board);
}

// Test material equality in starting position
TEST(EvaluationTest, StartingPositionIsSymmetric) {
    Board board;
    board.reset_to_start();

    // Since starting position is 100% symmetric, score must be exactly 0
    int score = evaluate(board);
    EXPECT_EQ(score, 0);
}

// Test position with extra White Queen
TEST(EvaluationTest, ExtraWhiteQueen) {
    Board board;
    
    // Position without Black Queen (White has extra Queen)
    std::string fen = "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(fen));

    int score = evaluate(board);
    EXPECT_GT(score, 800); // Queen material is 900, should be strongly positive
}

// Test position with extra White Pawn
TEST(EvaluationTest, ExtraWhitePawn) {
    Board board;
    
    // Position without Black pawn on E7 (White has extra Pawn)
    std::string fen = "rnbqkbnr/pppp1ppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(fen));

    int score = evaluate(board);
    EXPECT_GT(score, 50); // Pawn material is 100, should be positive
}

// Test King Safety pawn shield evaluation
TEST(EvaluationTest, KingSafetyPawnShield) {
    Board board_safe;
    Board board_unsafe;

    // Both boards are set up with castled King, but board_unsafe has missing pawns in front of the King.
    
    // White King castled on G1, Pawns on F2, G2, H2 are intact
    std::string safe_fen = "8/8/8/8/8/8/5PPP/6KR w - - 0 1";
    EXPECT_TRUE(board_safe.load_from_fen(safe_fen));

    // White King castled on G1, but F2 and G2 pawns are missing (unsafe shield)
    std::string unsafe_fen = "8/8/8/8/8/8/7P/6KR w - - 0 1";
    EXPECT_TRUE(board_unsafe.load_from_fen(unsafe_fen));

    int safe_score = evaluate(board_safe);
    int unsafe_score = evaluate(board_unsafe);

    // Unsafe board should evaluate lower due to king safety penalties
    // Material is different by 2 pawns (200 cp), king safety adds -40 cp penalty.
    // Total difference should exceed just the material difference.
    EXPECT_GT(safe_score, unsafe_score);
}

// Test Checkmate position detection and scoring
TEST(EvaluationTest, CheckmatePositionScoring) {
    Board board;

    // Scholar's Mate position (Black is checkmated)
    // Black King is on E8. White Queen on F7, White Bishop on C4.
    // Black has 0 legal moves and is in check.
    std::string mate_fen = "r1bqkbnr/pppp1Qpp/2n5/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4";
    EXPECT_TRUE(board.load_from_fen(mate_fen));
    EXPECT_TRUE(is_in_check(board, Color::Black));

    int score = get_game_state_score(board);
    EXPECT_EQ(score, 30000); // White wins (+30000)

    // Reverse checkmate (White is checkmated, Fool's mate)
    std::string fools_mate_fen = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3";
    EXPECT_TRUE(board.load_from_fen(fools_mate_fen));
    EXPECT_TRUE(is_in_check(board, Color::White));

    score = get_game_state_score(board);
    EXPECT_EQ(score, -30000); // Black wins (-30000)
}

// Test Stalemate position detection and scoring
TEST(EvaluationTest, StalematePositionScoring) {
    Board board;

    // Classic stalemate position (Black to move, Black King on H8, White Queen on G6, White King on F7)
    // Black King has no legal moves but is NOT in check.
    std::string stalemate_fen = "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1";
    EXPECT_TRUE(board.load_from_fen(stalemate_fen));
    EXPECT_FALSE(is_in_check(board, Color::Black));

    int score = get_game_state_score(board);
    EXPECT_EQ(score, 0); // Draw (0 score)
}
