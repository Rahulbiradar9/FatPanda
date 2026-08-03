#include <gtest/gtest.h>
#include "board/board.hpp"
#include "evaluation/evaluation.hpp"
#include "evaluation/params.hpp"

using namespace ChessEngine;

TEST(TuningTest, SerializationAndDeserialization) {
    // 1. Backup current parameters
    EvalParams backup = g_eval_params;
    
    // 2. Export parameters
    std::string exported = export_eval_params();
    EXPECT_FALSE(exported.empty());
    
    // 3. Mutate parameters
    g_eval_params.val_pawn = 185;
    g_eval_params.val_knight = 425;
    g_eval_params.pawn_pst[0] = 99;
    
    // 4. Reload from exported string
    bool success = load_eval_params(exported);
    ASSERT_TRUE(success);
    
    // 5. Verify parameters are restored to backup state
    EXPECT_EQ(g_eval_params.val_pawn, backup.val_pawn);
    EXPECT_EQ(g_eval_params.val_knight, backup.val_knight);
    EXPECT_EQ(g_eval_params.pawn_pst[0], backup.pawn_pst[0]);
    
    // Restore backup
    g_eval_params = backup;
}

TEST(TuningTest, EvaluationUpdate) {
    Board board;
    // Set up position where White is up by exactly 1 pawn on a2
    ASSERT_TRUE(board.loadFromFen("k7/8/8/8/8/8/P7/7K w - - 0 1"));
    
    EvalParams backup = g_eval_params;
    
    // 1. Evaluate with standard pawn value (100)
    g_eval_params.val_pawn = 100;
    int score1 = evaluate(board);
    
    // 2. Evaluate with modified pawn value (200)
    g_eval_params.val_pawn = 200;
    int score2 = evaluate(board);
    
    // 3. Score difference should reflect the pawn value increase (200 - 100 = 100)
    // Note: evaluate returns score from side-to-move's perspective (White), 
    // so score2 - score1 should be exactly 100!
    EXPECT_EQ(score2 - score1, 100);
    
    // Restore backup
    g_eval_params = backup;
}
