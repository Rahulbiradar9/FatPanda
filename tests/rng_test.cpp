#include <gtest/gtest.h>
#include "utils/rng.hpp"
#include <vector>

TEST(RNGTest, SeedDeterminism) {
    ChessEngine::set_global_seed(12345ULL);
    std::vector<uint64_t> seq1;
    for (int i = 0; i < 20; ++i) {
        seq1.push_back(ChessEngine::rand_u64());
    }

    ChessEngine::set_global_seed(12345ULL);
    std::vector<uint64_t> seq2;
    for (int i = 0; i < 20; ++i) {
        seq2.push_back(ChessEngine::rand_u64());
    }

    EXPECT_EQ(seq1, seq2);
}

TEST(RNGTest, DifferentSeedsProduceDifferentSequences) {
    ChessEngine::set_global_seed(12345ULL);
    std::vector<uint64_t> seq1;
    for (int i = 0; i < 10; ++i) {
        seq1.push_back(ChessEngine::rand_u64());
    }

    ChessEngine::set_global_seed(67890ULL);
    std::vector<uint64_t> seq2;
    for (int i = 0; i < 10; ++i) {
        seq2.push_back(ChessEngine::rand_u64());
    }

    EXPECT_NE(seq1, seq2);
}

TEST(RNGTest, RandRangeBounds) {
    ChessEngine::set_global_seed(42ULL);
    for (int i = 0; i < 1000; ++i) {
        int val = ChessEngine::rand_range(5, 15);
        EXPECT_GE(val, 5);
        EXPECT_LE(val, 15);
    }
}

TEST(RNGTest, RandIndexBounds) {
    ChessEngine::set_global_seed(42ULL);
    for (int i = 0; i < 1000; ++i) {
        size_t idx = ChessEngine::rand_index(10);
        EXPECT_LT(idx, 10u);
    }
}
