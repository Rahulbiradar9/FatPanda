#include <gtest/gtest.h>
#include "board/board.hpp"
#include "board/types.hpp"
#include "board/movegen.hpp"
#include <algorithm>

using namespace ChessEngine;

// Helper to check if a specific move exists in a vector of moves
bool move_exists(const std::vector<Move>& moves, Square from, Square to, uint16_t flags = MoveFlag::NORMAL) {
    for (const auto& m : moves) {
        if (m.get_from() == from && m.get_to() == to) {
            if (flags == MoveFlag::CAPTURE) {
                if (m.is_capture()) return true;
            } else if (flags == MoveFlag::NORMAL) {
                if (!m.is_capture() && !m.is_promo() && !m.is_double_push() && !m.is_en_passant() && !m.is_castle()) return true;
            } else {
                if (m.get_flags() == flags) return true;
            }
        }
    }
    return false;
}

// Test move generation in starting position
TEST(MoveGenTest, StartingPositionMovesCount) {
    Board board;
    board.reset_to_start();

    // White starting moves count must be exactly 20
    std::vector<Move> moves = generate_legal_moves(board);
    EXPECT_EQ(moves.size(), 20);

    // Verify some specific moves exist
    EXPECT_TRUE(move_exists(moves, Square::E2, Square::E3));
    EXPECT_TRUE(move_exists(moves, Square::E2, Square::E4, MoveFlag::DOUBLE_PUSH));
    EXPECT_TRUE(move_exists(moves, Square::G1, Square::F3));
    EXPECT_TRUE(move_exists(moves, Square::B1, Square::C3));
    EXPECT_FALSE(move_exists(moves, Square::E1, Square::E2)); // King cannot move to blocked square
}

// Test check validation and resolution
TEST(MoveGenTest, KingInCheckResolution) {
    Board board;
    
    // Position where White King is on E1, Black Rook is on E8 (giving check)
    // There's a White Bishop on D2 that can block, and White Rook on A1.
    // 1. King can move out of check (D1, F1).
    // 2. Bishop can block the check (E3).
    // 3. White Rook on A1 cannot make normal moves unless they resolve the check.
    std::string check_fen = "4r3/8/8/8/8/8/3B4/R3K3 w - - 0 1";
    EXPECT_TRUE(board.load_from_fen(check_fen));
    EXPECT_TRUE(is_in_check(board, Color::White));

    std::vector<Move> moves = generate_legal_moves(board);

    // Valid check escapes:
    // - King E1 to D1 (quiet)
    // - King E1 to F1 (quiet)
    // - King E1 to F2 (quiet)
    // - Bishop D2 to E3 (block check)
    // Total should be exactly those moves. Let's verify them:
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::D1));
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::F1));
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::F2));
    EXPECT_TRUE(move_exists(moves, Square::D2, Square::E3));

    // Non-escape moves (like Rook A1 to B1) must not be generated
    EXPECT_FALSE(move_exists(moves, Square::A1, Square::B1));

    // Total legal moves must be exactly 4
    EXPECT_EQ(moves.size(), 4);
}

// Test Castling generation and blocks
TEST(MoveGenTest, CastlingMovesAndBlocks) {
    Board board;

    // Custom FEN with White King on E1, Rooks on A1, H1. Board is clear between them.
    std::string castling_fen = "8/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(castling_fen));

    std::vector<Move> moves = generate_legal_moves(board);
    
    // Both castling moves must be generated
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::G1, MoveFlag::CASTLE_K));
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::C1, MoveFlag::CASTLE_Q));

    // Now place a Black Rook on G8, which attacks G1. White King-side castle is blocked.
    std::string blocked_fen = "6r1/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(blocked_fen));
    
    moves = generate_legal_moves(board);
    EXPECT_FALSE(move_exists(moves, Square::E1, Square::G1, MoveFlag::CASTLE_K));
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::C1, MoveFlag::CASTLE_Q));

    // Place a piece between Rook and King (e.g. Knight on B1)
    std::string occupied_fen = "8/8/8/8/8/8/8/RN2K2R w KQkq - 0 1";
    EXPECT_TRUE(board.load_from_fen(occupied_fen));

    moves = generate_legal_moves(board);
    EXPECT_FALSE(move_exists(moves, Square::E1, Square::C1, MoveFlag::CASTLE_Q)); // Blocked by Knight
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::G1, MoveFlag::CASTLE_K));  // Clear
}

// Test En-passant generation
TEST(MoveGenTest, EnPassantGeneration) {
    Board board;
    
    // Standard custom setup: White pawn on D5, Black pawn on E5.
    // Last move was e7e5 (double push), leaving en-passant on e6.
    std::string ep_fen = "rnbqkbnr/pppp1ppp/8/3Pp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2";
    EXPECT_TRUE(board.load_from_fen(ep_fen));

    std::vector<Move> moves = generate_legal_moves(board);

    // Verify White pawn can capture Black pawn via en-passant
    EXPECT_TRUE(move_exists(moves, Square::D5, Square::E6, MoveFlag::EN_PASSANT));

    // Verify after make_move, the captured Black pawn on E5 is removed!
    Move ep_move(Square::D5, Square::E6, MoveFlag::EN_PASSANT);
    EXPECT_TRUE(board.make_move(ep_move));
    EXPECT_EQ(board.get_piece(Square::E5), Piece::None);
    EXPECT_EQ(board.get_piece(Square::E6), Piece::WhitePawn);
}

// Test Pawn promotions
TEST(MoveGenTest, PawnPromotions) {
    Board board;

    // White pawn on E7. Single push to E8 is a promotion.
    std::string promo_fen = "8/4P3/8/8/8/8/8/4K3 w - - 0 1";
    EXPECT_TRUE(board.load_from_fen(promo_fen));

    std::vector<Move> moves = generate_legal_moves(board);

    // All 4 promotion options must be generated
    EXPECT_TRUE(move_exists(moves, Square::E7, Square::E8, MoveFlag::PROMO_N));
    EXPECT_TRUE(move_exists(moves, Square::E7, Square::E8, MoveFlag::PROMO_B));
    EXPECT_TRUE(move_exists(moves, Square::E7, Square::E8, MoveFlag::PROMO_R));
    EXPECT_TRUE(move_exists(moves, Square::E7, Square::E8, MoveFlag::PROMO_Q));

    // Normal non-promoting moves must not exist
    EXPECT_FALSE(move_exists(moves, Square::E7, Square::E8, MoveFlag::NORMAL));
}

// Test pseudo-legal move generation with camelCase helper
TEST(MoveGenTest, PseudoLegalMoveGenCamelCase) {
    Board board;
    board.setStartingPosition();

    // Verify starting position pseudo-legal moves
    std::vector<Move> moves = generatePseudoLegalMoves(board);
    EXPECT_EQ(moves.size(), 20);

    // Verify that a double push exists
    EXPECT_TRUE(move_exists(moves, Square::E2, Square::E4, MoveFlag::DOUBLE_PUSH));

    // Verify captured piece properties on a custom capture position
    // White pawn on d4, Black pawn on e5. d4xe5 is a capture.
    std::string capture_fen = "rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2";
    EXPECT_TRUE(board.loadFromFen(capture_fen));

    std::vector<Move> cap_moves = generatePseudoLegalMoves(board);
    bool found_capture = false;
    for (const auto& m : cap_moves) {
        if (m.getSourceSquare() == Square::D4 && m.getDestinationSquare() == Square::E5) {
            EXPECT_TRUE(m.isCapture());
            EXPECT_EQ(m.getCapturedPieceType(), PieceType::Pawn);
            found_capture = true;
        }
    }
    EXPECT_TRUE(found_capture);
}

// Test pinned pieces constraint: pinned piece cannot move off the line of defense
TEST(MoveGenTest, PinnedPiecesLegality) {
    Board board;
    // FEN: White King on e1, White Rook on e2, Black Rook on e8 (pinning the e2 rook)
    const std::string pinned_fen = "4r3/8/8/8/8/8/4R3/4K3 w - - 0 1";
    ASSERT_TRUE(board.loadFromFen(pinned_fen));

    std::vector<Move> moves = generateLegalMoves(board);

    // Rook on e2 moving to e3 or e8 (staying on e-file) is legal
    EXPECT_TRUE(move_exists(moves, Square::E2, Square::E3, MoveFlag::NORMAL));
    EXPECT_TRUE(move_exists(moves, Square::E2, Square::E8, MoveFlag::CAPTURE));

    // Rook on e2 moving to d2 or f2 (leaving e-file) is illegal
    EXPECT_FALSE(move_exists(moves, Square::E2, Square::D2, MoveFlag::NORMAL));
    EXPECT_FALSE(move_exists(moves, Square::E2, Square::F2, MoveFlag::NORMAL));
}

// Test double check constraint: only King moves are legal
TEST(MoveGenTest, DoubleCheckLegality) {
    Board board;
    // FEN: White King on e1, Black Queen on e2 (checking), Black Knight on c2 (checking)
    const std::string double_check_fen = "8/8/8/8/8/8/2n1q3/4K3 w - - 0 1";
    ASSERT_TRUE(board.loadFromFen(double_check_fen));

    std::vector<Move> moves = generateLegalMoves(board);

    // Only King moves must be generated. (e.g. King to d1, f1, or capturing e2)
    for (const auto& m : moves) {
        EXPECT_EQ(board.getPiece(m.getSourceSquare()), Piece::WhiteKing);
    }
    
    // King capturing Queen on e2 is legal (since Knight on c2 doesn't guard e2)
    EXPECT_TRUE(move_exists(moves, Square::E1, Square::E2, MoveFlag::CAPTURE));
}

// Test castling legality under check conditions
TEST(MoveGenTest, CastlingLegalityConstraints) {
    Board board;
    
    // 1. Cannot castle out of check
    // FEN: White King on e1 under check from Black Rook on e2.
    const std::string castle_check_fen = "4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1";
    ASSERT_TRUE(board.loadFromFen(castle_check_fen));
    // Verify King is in check
    ASSERT_TRUE(is_in_check(board, Color::White));
    std::vector<Move> moves1 = generateLegalMoves(board);
    EXPECT_FALSE(move_exists(moves1, Square::E1, Square::G1, MoveFlag::CASTLE_K));
    EXPECT_FALSE(move_exists(moves1, Square::E1, Square::C1, MoveFlag::CASTLE_Q));

    // 2. Cannot castle through check (f1 attacked)
    // FEN: Black Rook on f3 attacks f1.
    const std::string castle_through_fen = "4k3/8/8/8/8/5r2/8/R3K2R w KQ - 0 1";
    ASSERT_TRUE(board.loadFromFen(castle_through_fen));
    std::vector<Move> moves2 = generateLegalMoves(board);
    // Kingside castling (O-O) passes through f1, so it is illegal
    EXPECT_FALSE(move_exists(moves2, Square::E1, Square::G1, MoveFlag::CASTLE_K));
    // Queenside castling (O-O-O) is still legal
    EXPECT_TRUE(move_exists(moves2, Square::E1, Square::C1, MoveFlag::CASTLE_Q));

    // 3. Cannot castle into check (g1 attacked)
    // FEN: Black Rook on g3 attacks g1.
    const std::string castle_into_fen = "4k3/8/8/8/8/6r1/8/R3K2R w KQ - 0 1";
    ASSERT_TRUE(board.loadFromFen(castle_into_fen));
    std::vector<Move> moves3 = generateLegalMoves(board);
    EXPECT_FALSE(move_exists(moves3, Square::E1, Square::G1, MoveFlag::CASTLE_K));
}

// Test en-passant legality: en-passant capture cannot open a pin/check
TEST(MoveGenTest, EnPassantPinLegality) {
    Board board;
    // FEN: White King on e1, White Pawn on e5, Black Pawn on d5.
    // Black Rook on e8 pins the e-file. If White captures exd6 ep, the e-file opens up,
    // exposing the King on e1 to check from the Rook on e8.
    const std::string ep_pin_fen = "4r3/8/8/3pP3/8/8/8/4K3 w - d6 0 2";
    ASSERT_TRUE(board.loadFromFen(ep_pin_fen));

    std::vector<Move> moves = generateLegalMoves(board);

    // En-passant capture exd6 is illegal because it leaves the King in check
    EXPECT_FALSE(move_exists(moves, Square::E5, Square::D6, MoveFlag::EN_PASSANT));
}
