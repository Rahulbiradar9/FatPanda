#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/types.hpp"
#include "board/movegen.hpp"
#include "evaluation/evaluation.hpp"

using namespace ChessEngine;

// Helper to evaluate with checkmate/stalemate detection (returns score from side-to-move's perspective)
int get_game_state_score(Board& board) {
    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (is_in_check(board, board.get_side_to_move())) {
            // Checkmate! For side-to-move, checkmate is a loss (-30000)
            return -30000;
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
    EXPECT_GT(safe_score, unsafe_score);
}

// Test Checkmate position detection and scoring
TEST(EvaluationTest, CheckmatePositionScoring) {
    Board board;

    // Scholar's Mate position (Black is checkmated)
    std::string mate_fen = "r1bqkbnr/pppp1Qpp/2n5/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4";
    EXPECT_TRUE(board.load_from_fen(mate_fen));
    EXPECT_TRUE(is_in_check(board, Color::Black));

    int score = get_game_state_score(board);
    EXPECT_EQ(score, -30000); // Black is to move and checkmated: loss (-30000)

    // Reverse checkmate (White is checkmated, Fool's mate)
    std::string fools_mate_fen = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3";
    EXPECT_TRUE(board.load_from_fen(fools_mate_fen));
    EXPECT_TRUE(is_in_check(board, Color::White));

    score = get_game_state_score(board);
    EXPECT_EQ(score, -30000); // White is to move and checkmated: loss (-30000)
}

// Test Stalemate position detection and scoring
TEST(EvaluationTest, StalematePositionScoring) {
    Board board;

    // Classic stalemate position (Black to move, Black King on H8, White Queen on G6, White King on F7)
    std::string stalemate_fen = "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1";
    EXPECT_TRUE(board.load_from_fen(stalemate_fen));
    EXPECT_FALSE(is_in_check(board, Color::Black));

    int score = get_game_state_score(board);
    EXPECT_EQ(score, 0); // Draw (0 score)
}

// Test Bishop Pair Bonus
TEST(EvaluationTest, BishopPairBonus) {
    Board board_pair;
    Board board_single;

    // Board with two bishops vs single bishop
    EXPECT_TRUE(board_pair.load_from_fen("k7/8/8/8/8/8/8/2B1B2K w - - 0 1"));
    EXPECT_TRUE(board_single.load_from_fen("k7/8/8/8/8/8/8/2B4K w - - 0 1"));

    // Material difference is 1 Bishop (330), but with bishop pair bonus the difference should be 380
    int score_pair = evaluateMaterial(board_pair);
    int score_single = evaluateMaterial(board_single);
    EXPECT_EQ(score_pair - score_single, 380);
}

// Test Doubled Pawn Penalty
TEST(EvaluationTest, DoubledPawnPenalty) {
    Board board_normal;
    Board board_doubled;

    // White pawns connected vs doubled on A file
    EXPECT_TRUE(board_normal.load_from_fen("k7/8/8/8/8/8/P1P5/K7 w - - 0 1"));
    EXPECT_TRUE(board_doubled.load_from_fen("k7/8/8/8/8/P7/P7/K7 w - - 0 1"));

    int normal_pawn_score = evaluatePawnStructure(board_normal);
    int doubled_pawn_score = evaluatePawnStructure(board_doubled);

    // Doubled pawns should be penalized
    EXPECT_GT(normal_pawn_score, doubled_pawn_score);
}

// Test Isolated Pawn Penalty
TEST(EvaluationTest, IsolatedPawnPenalty) {
    Board board_connected;
    Board board_isolated;

    // White pawns connected vs isolated
    EXPECT_TRUE(board_connected.load_from_fen("k7/8/8/8/8/8/PP6/K7 w - - 0 1"));
    EXPECT_TRUE(board_isolated.load_from_fen("k7/8/8/8/8/8/P1P5/K7 w - - 0 1"));

    int connected_pawn_score = evaluatePawnStructure(board_connected);
    int isolated_pawn_score = evaluatePawnStructure(board_isolated);

    // Isolated pawns should be penalized
    EXPECT_GT(connected_pawn_score, isolated_pawn_score);
}

// Test Passed Pawn Bonus
TEST(EvaluationTest, PassedPawnBonus) {
    Board board_passed;
    Board board_blocked;

    // White passed pawn vs blocked pawn
    EXPECT_TRUE(board_passed.load_from_fen("k7/8/8/3P4/8/8/8/K7 w - - 0 1"));
    EXPECT_TRUE(board_blocked.load_from_fen("k7/8/3p4/3P4/8/8/8/K7 w - - 0 1"));

    int passed_score = evaluatePawnStructure(board_passed);
    int blocked_score = evaluatePawnStructure(board_blocked);

    // Passed pawn should receive a significant bonus
    EXPECT_GT(passed_score, blocked_score);
}

// Test Rook Activity (Open File and 7th Rank)
TEST(EvaluationTest, RookActivity) {
    Board board_active;
    Board board_inactive;

    // White Rook on open file & 7th rank vs Rook on blocked file/closed rank
    EXPECT_TRUE(board_active.load_from_fen("k7/r1R5/8/8/8/8/8/K7 w - - 0 1"));
    EXPECT_TRUE(board_inactive.load_from_fen("k7/r7/8/8/8/8/R7/K7 w - - 0 1"));

    int active_mobility = evaluateMobility(board_active);
    int inactive_mobility = evaluateMobility(board_inactive);

    // Active rook should get open file and 7th rank bonuses
    EXPECT_GT(active_mobility, inactive_mobility);
}
