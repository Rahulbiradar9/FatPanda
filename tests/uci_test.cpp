#include <gtest/gtest.h>
#include "board/board.hpp"
#include "uci/uci.hpp"
#include "search/search.hpp"
#include <sstream>

using namespace ChessEngine;

TEST(UciTest, ParseMoveBasic) {
    Board board;
    board.reset_to_start();

    // Valid starting move
    Move m = parse_move(board, "e2e4");
    EXPECT_FALSE(m.is_none());
    EXPECT_EQ(m.getSourceSquare(), Square::E2);
    EXPECT_EQ(m.getDestinationSquare(), Square::E4);
    EXPECT_TRUE(m.isDoublePawnPush());

    // Invalid move format
    Move invalid = parse_move(board, "e2e9");
    EXPECT_TRUE(invalid.is_none());

    // Legal knight move
    Move knight_move = parse_move(board, "g1f3");
    EXPECT_FALSE(knight_move.is_none());
    EXPECT_EQ(knight_move.getSourceSquare(), Square::G1);
    EXPECT_EQ(knight_move.getDestinationSquare(), Square::F3);
}

TEST(UciTest, ParseMovePromotionAndSpecial) {
    Board board;
    // FEN with pawn on 7th rank ready to promote
    ASSERT_TRUE(board.loadFromFen("8/4P3/8/8/8/8/8/4K3 w - - 0 1"));

    // Underpromotion to Knight
    Move promo_n = parse_move(board, "e7e8n");
    EXPECT_FALSE(promo_n.is_none());
    EXPECT_TRUE(promo_n.isPromotion());
    EXPECT_EQ(promo_n.getPromotionPieceType(), PieceType::Knight);

    // Promotion to Queen
    Move promo_q = parse_move(board, "e7e8q");
    EXPECT_FALSE(promo_q.is_none());
    EXPECT_TRUE(promo_q.isPromotion());
    EXPECT_EQ(promo_q.getPromotionPieceType(), PieceType::Queen);

    // Castling
    ASSERT_TRUE(board.loadFromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    Move castle_k = parse_move(board, "e1g1");
    EXPECT_FALSE(castle_k.is_none());
    EXPECT_TRUE(castle_k.isCastling());
}

TEST(UciTest, ParsePositionStartpos) {
    Board board;
    std::stringstream ss("startpos moves e2e4 e7e5");
    parse_position(board, ss);

    EXPECT_EQ(board.getPiece(Square::E4), Piece::WhitePawn);
    EXPECT_EQ(board.getPiece(Square::E5), Piece::BlackPawn);
    EXPECT_EQ(board.get_side_to_move(), Color::White); // after 1. e4 e5 it is White's turn
}

TEST(UciTest, ParsePositionFen) {
    Board board;
    std::stringstream ss("fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves e1c1");
    parse_position(board, ss);

    // After e1c1 (queenside castle), white king is on c1, white rook on d1
    EXPECT_EQ(board.getPiece(Square::C1), Piece::WhiteKing);
    EXPECT_EQ(board.getPiece(Square::D1), Piece::WhiteRook);
}

TEST(UciTest, MultiPVSearch) {
    Board board;
    board.reset_to_start();
    g_multipv = 3;
    
    SearchResult res = search(board, 3);
    EXPECT_FALSE(res.best_move.is_none());
    EXPECT_GT(res.nodes_searched, 0u);
    
    g_multipv = 1; // Reset to default
}

TEST(UciTest, PonderOptionToggle) {
    std::stringstream ss1("name Ponder value false");
    parse_setoption(ss1);
    EXPECT_FALSE(g_ponder_enabled);

    std::stringstream ss2("name Ponder value true");
    parse_setoption(ss2);
    EXPECT_TRUE(g_ponder_enabled);
}

TEST(UciTest, GoPonderAndPonderHit) {
    Board board;
    board.reset_to_start();
    
    // go ponder depth 10
    std::stringstream ss("depth 10 ponder");
    parse_go(board, ss);
    
    EXPECT_TRUE(g_is_pondering.load());
    EXPECT_EQ(g_time_limit_soft_ms, -1);
    EXPECT_EQ(g_time_limit_hard_ms, -1);
    
    // Simulate ponderhit
    handle_ponderhit();
    EXPECT_FALSE(g_is_pondering.load());
    
    join_search_thread();
}

TEST(UciTest, GoPonderAndStop) {
    Board board;
    board.reset_to_start();
    
    std::stringstream ss("depth 10 ponder");
    parse_go(board, ss);
    
    EXPECT_TRUE(g_is_pondering.load());
    
    // Simulate stop command
    join_search_thread();
    EXPECT_FALSE(g_is_pondering.load());
}


