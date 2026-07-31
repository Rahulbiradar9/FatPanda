#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/types.hpp"

using namespace ChessEngine;

// Test standard starting position FEN round-trip
TEST(FenTest, StartPositionRoundTrip) {
    Board board;
    const std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    EXPECT_TRUE(board.load_from_fen(start_fen));
    EXPECT_EQ(board.to_fen(), start_fen);

    // Verify some board details
    EXPECT_EQ(board.get_piece(Square::E1), Piece::WhiteKing);
    EXPECT_EQ(board.get_piece(Square::E8), Piece::BlackKing);
    EXPECT_EQ(board.get_side_to_move(), Color::White);
    EXPECT_EQ(board.get_castling_rights(), Castling::ALL);
}

// Test custom positions with castling rights
TEST(FenTest, CastlingRightsRoundTrip) {
    Board board;
    
    // Partially missing castling rights
    std::string fen1 = "r3k2r/8/8/8/8/8/8/R3K2R w Qk - 0 1";
    EXPECT_TRUE(board.load_from_fen(fen1));
    EXPECT_EQ(board.to_fen(), fen1);
    EXPECT_EQ(board.get_castling_rights(), Castling::WQ | Castling::BK);

    // No castling rights
    std::string fen2 = "r3k2r/8/8/8/8/8/8/R3K2R w - - 4 12";
    EXPECT_TRUE(board.load_from_fen(fen2));
    EXPECT_EQ(board.to_fen(), fen2);
    EXPECT_EQ(board.get_castling_rights(), Castling::NONE);
    EXPECT_EQ(board.get_halfmove_clock(), 4);
    EXPECT_EQ(board.get_fullmove_number(), 12);
}

// Test positions with en-passant square
TEST(FenTest, EnPassantRoundTrip) {
    Board board;
    
    // EP on e3
    std::string fen_ep_w = "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    EXPECT_TRUE(board.load_from_fen(fen_ep_w));
    EXPECT_EQ(board.to_fen(), fen_ep_w);
    EXPECT_EQ(board.get_en_passant(), Square::E3);
    EXPECT_EQ(board.get_side_to_move(), Color::Black);

    // EP on f6
    std::string fen_ep_b = "rnbqkbnr/pp1ppppp/8/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq f6 1 3";
    EXPECT_TRUE(board.load_from_fen(fen_ep_b));
    EXPECT_EQ(board.to_fen(), fen_ep_b);
    EXPECT_EQ(board.get_en_passant(), Square::F6);
}

// Test positions with promoted/multiple pieces
TEST(FenTest, PromotedPiecesRoundTrip) {
    Board board;
    
    // Position with 3 white queens and 2 black queens
    std::string fen = "q3k2q/8/8/3Q4/4Q3/8/8/R3K2R b KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(fen));
    EXPECT_EQ(board.to_fen(), fen);
    
    EXPECT_EQ(board.get_piece(Square::A8), Piece::BlackQueen);
    EXPECT_EQ(board.get_piece(Square::H8), Piece::BlackQueen);
    EXPECT_EQ(board.get_piece(Square::D5), Piece::WhiteQueen);
    EXPECT_EQ(board.get_piece(Square::E4), Piece::WhiteQueen);
}

// Test parsing invalid FEN strings
TEST(FenTest, InvalidFenParsing) {
    Board board;

    // Malformed piece placement (too few ranks)
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));

    // Malformed rank (exceeds file count 8)
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/ppppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));

    // Malformed rank (too few files)
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));

    // Invalid active color
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"));

    // Invalid castling character
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQXk - 0 1"));

    // Invalid en-passant square format
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e9 0 1"));

    // Invalid halfmove clock
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - -1 1"));

    // Invalid fullmove number
    EXPECT_FALSE(board.load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0"));
}
