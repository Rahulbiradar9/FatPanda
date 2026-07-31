#include <gtest/gtest.h>
#include "move/move.hpp"
#include "board/types.hpp"

using namespace ChessEngine;

// Test Move creation and accessors
TEST(MoveTest, BasicMoveCreationAndAccessors) {
    Move m(Square::E2, Square::E4, MoveFlag::NORMAL);

    EXPECT_EQ(m.get_from(), Square::E2);
    EXPECT_EQ(m.get_to(), Square::E4);
    EXPECT_EQ(m.get_flags(), MoveFlag::NORMAL);
    EXPECT_FALSE(m.is_none());
    EXPECT_FALSE(m.is_capture());
    EXPECT_FALSE(m.is_promo());
    EXPECT_FALSE(m.is_double_push());
    EXPECT_FALSE(m.is_castle());
    EXPECT_FALSE(m.is_en_passant());
    EXPECT_EQ(m.to_string(), "e2e4");
}

// Test Capture Move flags
TEST(MoveTest, CaptureMoveFlags) {
    Move m(Square::D5, Square::E6, MoveFlag::CAPTURE);

    EXPECT_EQ(m.get_from(), Square::D5);
    EXPECT_EQ(m.get_to(), Square::E6);
    EXPECT_TRUE(m.is_capture());
    EXPECT_FALSE(m.is_promo());
    EXPECT_FALSE(m.is_en_passant());
    EXPECT_EQ(m.to_string(), "d5e6");
}

// Test Promotion Move flags and promotion piece extraction
TEST(MoveTest, PromotionMoveFlags) {
    // White Knight Promotion
    Move promo_n(Square::G7, Square::G8, MoveFlag::PROMO_N);
    EXPECT_TRUE(promo_n.is_promo());
    EXPECT_FALSE(promo_n.is_capture());
    EXPECT_EQ(promo_n.get_promotion_piece_type(), PieceType::Knight);
    EXPECT_EQ(promo_n.to_string(), "g7g8n");

    // Black Queen Promotion + Capture
    Move promo_q_cap(Square::C2, Square::D1, MoveFlag::PROMO_Q_CAP);
    EXPECT_TRUE(promo_q_cap.is_promo());
    EXPECT_TRUE(promo_q_cap.is_capture());
    EXPECT_EQ(promo_q_cap.get_promotion_piece_type(), PieceType::Queen);
    EXPECT_EQ(promo_q_cap.to_string(), "c2d1q");

    // White Rook Promotion
    Move promo_r(Square::A7, Square::A8, MoveFlag::PROMO_R);
    EXPECT_TRUE(promo_r.is_promo());
    EXPECT_EQ(promo_r.get_promotion_piece_type(), PieceType::Rook);
    EXPECT_EQ(promo_r.to_string(), "a7a8r");

    // White Bishop Promotion + Capture
    Move promo_b_cap(Square::B7, Square::C8, MoveFlag::PROMO_B_CAP);
    EXPECT_TRUE(promo_b_cap.is_promo());
    EXPECT_TRUE(promo_b_cap.is_capture());
    EXPECT_EQ(promo_b_cap.get_promotion_piece_type(), PieceType::Bishop);
    EXPECT_EQ(promo_b_cap.to_string(), "b7c8b");
}

// Test special moves: Double Pawn Push, Castle, En Passant
TEST(MoveTest, SpecialMoveFlags) {
    // Double Pawn Push
    Move double_push(Square::E2, Square::E4, MoveFlag::DOUBLE_PUSH);
    EXPECT_TRUE(double_push.is_double_push());
    EXPECT_FALSE(double_push.is_castle());
    EXPECT_FALSE(double_push.is_en_passant());
    EXPECT_EQ(double_push.to_string(), "e2e4");

    // King-side Castle
    Move castle_k(Square::E1, Square::G1, MoveFlag::CASTLE_K);
    EXPECT_TRUE(castle_k.is_castle());
    EXPECT_TRUE(castle_k.is_castle_k());
    EXPECT_FALSE(castle_k.is_castle_q());
    EXPECT_FALSE(castle_k.is_en_passant());
    EXPECT_EQ(castle_k.to_string(), "e1g1");

    // Queen-side Castle
    Move castle_q(Square::E8, Square::C8, MoveFlag::CASTLE_Q);
    EXPECT_TRUE(castle_q.is_castle());
    EXPECT_TRUE(castle_q.is_castle_q());
    EXPECT_FALSE(castle_q.is_castle_k());
    EXPECT_EQ(castle_q.to_string(), "e8c8");

    // En Passant Capture
    Move ep(Square::E5, Square::D6, MoveFlag::EN_PASSANT);
    EXPECT_TRUE(ep.is_en_passant());
    EXPECT_TRUE(ep.is_capture()); // EP is a capture!
    EXPECT_FALSE(ep.is_double_push());
    EXPECT_EQ(ep.to_string(), "e5d6");
}

// Test none/null move behavior
TEST(MoveTest, NoneMoveBehavior) {
    Move m = MOVE_NONE;
    EXPECT_TRUE(m.is_none());
    EXPECT_EQ(m.get_from(), Square::A1); // index 0
    EXPECT_EQ(m.get_to(), Square::A1);
    EXPECT_EQ(m.to_string(), "0000");
}

// Test compile-time constexpr move encoding and extraction
TEST(MoveTest, ConstexprMoveVerification) {
    constexpr Move m(Square::F2, Square::F3, MoveFlag::NORMAL);
    static_assert(m.get_from() == Square::F2, "From square check");
    static_assert(m.get_to() == Square::F3, "To square check");
    static_assert(m.get_flags() == MoveFlag::NORMAL, "Flags check");
    static_assert(!m.is_capture(), "Capture flag check");

    constexpr Move promo_cap(Square::H7, Square::G8, MoveFlag::PROMO_Q_CAP);
    static_assert(promo_cap.is_promo(), "Is promo check");
    static_assert(promo_cap.is_capture(), "Is capture check");
    static_assert(promo_cap.get_promotion_piece_type() == PieceType::Queen, "Promo type check");
}
