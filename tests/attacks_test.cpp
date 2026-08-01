#include <gtest/gtest.h>
#include "board/attacks.hpp"
#include "board/bitboard.hpp"

using namespace ChessEngine;

// Verify Pawn attacks from every square
TEST(AttacksTest, PawnAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        int file = get_file(sq);
        int rank = get_rank(sq);

        // White Pawns
        Bitboard w_attacks = getPawnAttacks(sq, Color::White);
        if (rank == 7) {
            // Pawns on rank 8 cannot attack anything forwards
            EXPECT_EQ(w_attacks, EMPTY_BOARD);
        } else {
            int expected_count = 0;
            if (file > 0) {
                EXPECT_TRUE(test_bit(w_attacks, make_square(file - 1, rank + 1)));
                expected_count++;
            }
            if (file < 7) {
                EXPECT_TRUE(test_bit(w_attacks, make_square(file + 1, rank + 1)));
                expected_count++;
            }
            EXPECT_EQ(count_bits(w_attacks), expected_count);
        }

        // Black Pawns
        Bitboard b_attacks = getPawnAttacks(sq, Color::Black);
        if (rank == 0) {
            // Pawns on rank 1 cannot attack anything backwards
            EXPECT_EQ(b_attacks, EMPTY_BOARD);
        } else {
            int expected_count = 0;
            if (file > 0) {
                EXPECT_TRUE(test_bit(b_attacks, make_square(file - 1, rank - 1)));
                expected_count++;
            }
            if (file < 7) {
                EXPECT_TRUE(test_bit(b_attacks, make_square(file + 1, rank - 1)));
                expected_count++;
            }
            EXPECT_EQ(count_bits(b_attacks), expected_count);
        }
    }
}

// Verify Knight attacks from every square
TEST(AttacksTest, KnightAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        int file = get_file(sq);
        int rank = get_rank(sq);

        Bitboard attacks = getKnightAttacks(sq);
        
        // Count possible jumps manually
        int expected_count = 0;
        const int dr[] = { -2, -2, -1, -1, 1, 1, 2, 2 };
        const int df[] = { -1, 1, -2, 2, -2, 2, -1, 1 };

        for (int i = 0; i < 8; ++i) {
            int nr = rank + dr[i];
            int nf = file + df[i];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                EXPECT_TRUE(test_bit(attacks, make_square(nf, nr)));
                expected_count++;
            }
        }
        EXPECT_EQ(count_bits(attacks), expected_count);
    }
}

// Verify King attacks from every square
TEST(AttacksTest, KingAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        int file = get_file(sq);
        int rank = get_rank(sq);

        Bitboard attacks = getKingAttacks(sq);
        
        int expected_count = 0;
        const int dr[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        const int df[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

        for (int i = 0; i < 8; ++i) {
            int nr = rank + dr[i];
            int nf = file + df[i];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                EXPECT_TRUE(test_bit(attacks, make_square(nf, nr)));
                expected_count++;
            }
        }
        EXPECT_EQ(count_bits(attacks), expected_count);
    }
}

// Verify Bishop sliding attacks from every square (empty and blocked boards)
TEST(AttacksTest, BishopAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        int file = get_file(sq);
        int rank = get_rank(sq);

        // Empty Board Bishop attacks
        Bitboard empty_attacks = getBishopAttacks(sq, EMPTY_BOARD);
        
        // Blocked Board Bishop attacks: place blockers on diagonals
        Bitboard occupancy = EMPTY_BOARD;
        if (file > 1 && rank > 1) set_bit(occupancy, make_square(file - 2, rank - 2));
        if (file < 6 && rank < 6) set_bit(occupancy, make_square(file + 2, rank + 2));

        Bitboard blocked_attacks = getBishopAttacks(sq, occupancy);
        
        // Verify blocked attacks are a subset of empty attacks
        EXPECT_EQ(blocked_attacks & ~empty_attacks, EMPTY_BOARD);

        // Verify blockers cap the rays
        if (file > 1 && rank > 1) {
            EXPECT_TRUE(test_bit(blocked_attacks, make_square(file - 2, rank - 2)));
            EXPECT_FALSE(test_bit(blocked_attacks, make_square(file - 3, rank - 3))); // blocked!
        }
        if (file < 6 && rank < 6) {
            EXPECT_TRUE(test_bit(blocked_attacks, make_square(file + 2, rank + 2)));
            EXPECT_FALSE(test_bit(blocked_attacks, make_square(file + 3, rank + 3))); // blocked!
        }
    }
}

// Verify Rook sliding attacks from every square (empty and blocked boards)
TEST(AttacksTest, RookAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        int file = get_file(sq);
        int rank = get_rank(sq);

        // Empty Board Rook attacks should always have exactly 14 squares
        Bitboard empty_attacks = getRookAttacks(sq, EMPTY_BOARD);
        EXPECT_EQ(count_bits(empty_attacks), 14);

        // Blocked Board Rook attacks: place blockers at distance 2 in all 4 directions
        Bitboard occupancy = EMPTY_BOARD;
        if (file > 1) set_bit(occupancy, make_square(file - 2, rank));
        if (file < 6) set_bit(occupancy, make_square(file + 2, rank));
        if (rank > 1) set_bit(occupancy, make_square(file, rank - 2));
        if (rank < 6) set_bit(occupancy, make_square(file, rank + 2));

        Bitboard blocked_attacks = getRookAttacks(sq, occupancy);

        // Verify blockers cap the rays
        if (file > 1) {
            EXPECT_TRUE(test_bit(blocked_attacks, make_square(file - 2, rank)));
            EXPECT_FALSE(test_bit(blocked_attacks, make_square(file - 3, rank)));
        }
        if (file < 6) {
            EXPECT_TRUE(test_bit(blocked_attacks, make_square(file + 2, rank)));
            EXPECT_FALSE(test_bit(blocked_attacks, make_square(file + 3, rank)));
        }
    }
}

// Verify Queen attacks are union of Rook and Bishop attacks
TEST(AttacksTest, QueenAttacksEverySquare) {
    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        
        Bitboard occupancy = 0x00F00F00F00F00F0ULL; // arbitrary occupancy
        Bitboard queen = getQueenAttacks(sq, occupancy);
        Bitboard rook = getRookAttacks(sq, occupancy);
        Bitboard bishop = getBishopAttacks(sq, occupancy);

        EXPECT_EQ(queen, rook | bishop);
    }
}
