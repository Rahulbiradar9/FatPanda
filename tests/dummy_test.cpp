#include <gtest/gtest.h>
#include "utils/version.hpp"

// Test case to verify sanity of environment and library linking
TEST(SanityTest, VersionStringIsCorrect) {
    std::string version = ChessEngine::get_version_string();
    EXPECT_FALSE(version.empty());
    EXPECT_TRUE(version.rfind("0.1.1", 0) == 0);
}
