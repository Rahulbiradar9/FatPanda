include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Declare GoogleTest dependency
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)

# For Windows: prevent GTest from overriding compiler runtime settings (/MT vs /MD mismatch)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Download and load dependency
FetchContent_MakeAvailable(googletest)

# Define the unit tests executable
add_executable(FatPandaTests
    dummy_test.cpp
    bitboard_test.cpp
    board_test.cpp
    fen_test.cpp
    move_test.cpp
    movegen_test.cpp
    evaluation_test.cpp
    search_test.cpp
    chess_types_test.cpp
    bitboard_new_test.cpp
    attacks_test.cpp
    perft_test.cpp
    hash_test.cpp
)

# Link tests with googletest (gtest_main provides main()) and our engine logic library
target_link_libraries(FatPandaTests
    PRIVATE
        gtest_main
        FatPandaLib
)

# Enable CTest discovery for GTest
include(GoogleTest)
gtest_discover_tests(FatPandaTests)
