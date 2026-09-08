#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/movegen.hpp"
#include "uci/uci.hpp"

using namespace ChessEngine;

TEST(Chess960Test, ParseShredderFen) {
    Board board;
    // Starting FRC position #512: RNBBQKNR (King on e1, rooks on a1 and h1 -> standard)
    // Position #0: BBQNNRKR (King on g1, rooks on f1 and h1)
    std::string fen = "bbqnnrkr/pppppppp/8/8/8/8/PPPPPPPP/BBQNNRKR w HFhf - 0 1";
    ASSERT_TRUE(board.load_from_fen(fen));
    
    EXPECT_EQ(board.get_piece(Square::G1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::F1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::H1), Piece::WhiteRook);
    
    EXPECT_EQ(board.get_castling_rook(Color::White, true), Square::H1);
    EXPECT_EQ(board.get_castling_rook(Color::White, false), Square::F1);
    EXPECT_EQ(board.get_castling_rook(Color::Black, true), Square::H8);
    EXPECT_EQ(board.get_castling_rook(Color::Black, false), Square::F8);
}

TEST(Chess960Test, CastlingMovegenAndExecution) {
    Board board;
    g_chess960 = true;
    // Position with King on d1, Queenside Rook on a1, Kingside Rook on h1
    // RNBQKBNR -> r1bqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R1BQKBNR w AHah - 0 1
    // Clear d-file and c-file / f-file to allow castling
    ASSERT_TRUE(board.load_from_fen("r3k2r/8/8/8/8/8/8/R2K3R w AHah - 0 1"));
    
    auto legal_moves = generate_legal_moves(board);
    
    Move ks_castle = MOVE_NONE;
    Move qs_castle = MOVE_NONE;
    for (Move m : legal_moves) {
        if (m.is_castle_k()) ks_castle = m;
        if (m.is_castle_q()) qs_castle = m;
    }
    
    EXPECT_FALSE(ks_castle.is_none());
    EXPECT_FALSE(qs_castle.is_none());
    
    // In Chess960, source is king (d1) and destination is rook (h1 for KS, a1 for QS)
    EXPECT_EQ(ks_castle.getSourceSquare(), Square::D1);
    EXPECT_EQ(ks_castle.getDestinationSquare(), Square::H1);
    EXPECT_EQ(qs_castle.getSourceSquare(), Square::D1);
    EXPECT_EQ(qs_castle.getDestinationSquare(), Square::A1);
    
    // Test Kingside Castling execution
    UndoState undo;
    EXPECT_TRUE(board.makeMove(ks_castle, undo));
    EXPECT_EQ(board.get_piece(Square::G1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::F1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::D1), Piece::None);
    EXPECT_EQ(board.get_piece(Square::H1), Piece::None);
    
    board.unmakeMove(ks_castle, undo);
    EXPECT_EQ(board.get_piece(Square::D1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::H1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::G1), Piece::None);
    EXPECT_EQ(board.get_piece(Square::F1), Piece::None);
    
    // Test Queenside Castling execution
    EXPECT_TRUE(board.makeMove(qs_castle, undo));
    EXPECT_EQ(board.get_piece(Square::C1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::D1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::A1), Piece::None);
    
    board.unmakeMove(qs_castle, undo);
    EXPECT_EQ(board.get_piece(Square::D1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::A1), Piece::WhiteRook);
    EXPECT_EQ(board.get_piece(Square::C1), Piece::None);

    g_chess960 = false;
}

TEST(Chess960Test, MoveToStringNotation) {
    Move m(Square::E1, Square::H1, PieceType::None, PieceType::None, MoveFlag::CASTLE_K);
    
    g_chess960 = false;
    EXPECT_EQ(m.to_string(), "e1g1");
    
    g_chess960 = true;
    EXPECT_EQ(m.to_string(), "e1h1");
    
    g_chess960 = false; // reset
}

TEST(Chess960Test, ParseMoveBothFormats) {
    Board board;
    ASSERT_TRUE(board.load_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    
    // Standard format
    Move m1 = parse_move(board, "e1g1");
    EXPECT_FALSE(m1.is_none());
    EXPECT_TRUE(m1.is_castle_k());
    
    // Chess960 format (king captures rook)
    Move m2 = parse_move(board, "e1h1");
    EXPECT_FALSE(m2.is_none());
    EXPECT_TRUE(m2.is_castle_k());
    
    EXPECT_EQ(m1, m2);
}
