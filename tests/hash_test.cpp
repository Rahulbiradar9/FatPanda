#include <gtest/gtest.h>
#include "board/board.hpp"
#include "hash/zobrist.hpp"
#include "hash/tt.hpp"
#include "board/movegen.hpp"
#include "search/search.hpp"

using namespace ChessEngine;

TEST(HashTest, ZobristKeysInitialization) {
    initialize_zobrist_keys();
    
    // Check that keys are not all zero
    EXPECT_NE(side_key, 0ULL);
    
    bool all_zero = true;
    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            if (piece_keys[p][sq] != 0ULL) {
                all_zero = false;
                break;
            }
        }
    }
    EXPECT_FALSE(all_zero);
}

TEST(HashTest, IncrementalHashingMatchesComputeFromScratch) {
    Board board;
    board.reset_to_start();
    
    // Hash key from reset_to_start must equal compute_hash_key()
    uint64_t initial_hash = board.get_hash_key();
    uint64_t computed_hash = board.compute_hash_key();
    EXPECT_EQ(initial_hash, computed_hash);
    EXPECT_NE(initial_hash, 0ULL);
    
    // Generate legal moves and make a few moves, verifying incremental hash consistency
    auto moves = generate_legal_moves(board);
    ASSERT_FALSE(moves.empty());
    
    Move m = moves[0];
    UndoState undo;
    
    // Make move
    ASSERT_TRUE(board.makeMove(m, undo));
    EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
    EXPECT_NE(board.get_hash_key(), initial_hash); // Hash should have changed
    
    // Unmake move
    board.unmakeMove(m, undo);
    EXPECT_EQ(board.get_hash_key(), initial_hash); // Should be fully restored
    EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
}

TEST(HashTest, IncrementalHashingSequenceOfMoves) {
    Board board;
    board.reset_to_start();
    
    std::vector<Move> move_history;
    std::vector<UndoState> undo_history;
    
    // Play 5 legal moves and verify hash correctness at each step
    for (int step = 0; step < 5; ++step) {
        auto moves = generate_legal_moves(board);
        if (moves.empty()) break;
        
        Move m = moves[0];
        UndoState undo;
        if (board.makeMove(m, undo)) {
            move_history.push_back(m);
            undo_history.push_back(undo);
            EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
        }
    }
    
    // Undo all moves in reverse order and verify hash is restored
    for (int step = static_cast<int>(move_history.size()) - 1; step >= 0; --step) {
        board.unmakeMove(move_history[step], undo_history[step]);
        EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
    }
    
    // Check we are back to starting position hash
    Board starting_board;
    starting_board.reset_to_start();
    EXPECT_EQ(board.get_hash_key(), starting_board.get_hash_key());
}

TEST(HashTest, HashConsistencyOnFenLoad) {
    Board board;
    
    // Standard starting position
    ASSERT_TRUE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
    
    // Complex middlegame position
    const std::string complex_fen = "r1b1k2r/pp2bppp/2n1pn2/1B1p4/3NP3/2N1BP2/PqP3PP/R2QK2R w KQkq - 0 10";
    ASSERT_TRUE(board.load_from_fen(complex_fen));
    EXPECT_EQ(board.get_hash_key(), board.compute_hash_key());
}

TEST(HashTest, TranspositionTableProbingAndRecording) {
    TranspositionTable tt(1); // 1 MB table
    tt.clear();
    
    uint64_t test_key = 0x123456789ABCDEF0ULL;
    TTEntry entry;
    
    // Probe empty table
    EXPECT_FALSE(tt.probe(test_key, 0, entry));
    
    // Record and retrieve normal entry
    Move test_move(Square::E2, Square::E4, MoveFlag::DOUBLE_PUSH);
    tt.record(test_key, test_move, 150, 4, TT_EXACT, 2);
    
    EXPECT_TRUE(tt.probe(test_key, 2, entry));
    EXPECT_EQ(entry.key, test_key);
    EXPECT_EQ(entry.move, test_move);
    EXPECT_EQ(entry.score, 150);
    EXPECT_EQ(entry.depth, 4);
    EXPECT_EQ(entry.flags, TT_EXACT);
}

TEST(HashTest, TranspositionTableMateScoreAdjustment) {
    TranspositionTable tt(1);
    tt.clear();
    
    uint64_t test_key = 0x9876543210FEDCBAULL;
    Move test_move(Square::D7, Square::D8, MoveFlag::PROMO_Q);
    
    // Mate in 3 moves (6 plies) found at ply 2.
    // Score is MATE_SCORE - 6.
    int search_mate_score = MATE_SCORE - 6;
    
    // Record at ply 2
    tt.record(test_key, test_move, search_mate_score, 5, TT_EXACT, 2);
    
    // Probe at ply 2 (should return the exact same score MATE_SCORE - 6)
    TTEntry entry;
    EXPECT_TRUE(tt.probe(test_key, 2, entry));
    EXPECT_EQ(entry.score, search_mate_score);
    
    // Probe at ply 4 (should return MATE_SCORE - 8)
    EXPECT_TRUE(tt.probe(test_key, 4, entry));
    EXPECT_EQ(entry.score, MATE_SCORE - 8);
    
    // Probe at ply 0 (should return MATE_SCORE - 4)
    EXPECT_TRUE(tt.probe(test_key, 0, entry));
    EXPECT_EQ(entry.score, MATE_SCORE - 4);
}
