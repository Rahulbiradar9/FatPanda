cmake_minimum_required(VERSION 3.14)
project(FatPanda VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing globally
enable_testing()

# Extract git commit hash for versioning and OpenBench determinism
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "799ee98")
endif()
add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

# Define the modular Chess Engine Library
add_library(FatPandaLib STATIC
    src/utils/version.cpp
    src/utils/chess_types.cpp
    src/utils/rng.cpp
    src/board/attacks.cpp
    src/board/board.cpp
    src/board/bitboard.cpp
    src/board/fen.cpp
    src/move/move.cpp
    src/board/movegen.cpp
    src/board/perft.cpp
    src/board/polyglot.cpp
    src/board/syzygy.cpp
    src/evaluation/evaluation.cpp
    src/evaluation/params.cpp
    src/evaluation/nnue.cpp
    src/search/search.cpp
    src/hash/zobrist.cpp
    src/hash/tt.cpp
    src/uci/uci.cpp
)

# Set include directories
target_include_directories(FatPandaLib PUBLIC src)

# Set warnings and SIMD flags for FatPandaLib target
if(MSVC)
    target_compile_options(FatPandaLib PRIVATE /W4 /permissive- /arch:AVX2)
else()
    target_compile_options(FatPandaLib PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -mavx2 -mbmi2)
endif()

# Define the Main Executable
add_executable(FatPanda src/main.cpp)
target_link_libraries(FatPanda PRIVATE FatPandaLib)

# Set warnings and SIMD flags for FatPanda executable target
if(MSVC)
    target_compile_options(FatPanda PRIVATE /W4 /permissive- /arch:AVX2)
else()
    target_compile_options(FatPanda PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -mavx2 -mbmi2)
endif()

# Profile-Guided Optimization (PGO) Configuration
set(ENABLE_PGO "OFF" CACHE STRING "Enable Profile Guided Optimization (OFF, GENERATE, APPLY)")
set_property(CACHE ENABLE_PGO PROPERTY STRINGS "OFF" "GENERATE" "APPLY")

if(ENABLE_PGO STREQUAL "GENERATE")
    message(STATUS "PGO: Profile Generation Mode enabled")
    if(MSVC)
        target_compile_options(FatPandaLib PRIVATE /GL)
        target_compile_options(FatPanda PRIVATE /GL)
        target_link_options(FatPanda PRIVATE /LTCG /GENPROFILE)
    else()
        target_compile_options(FatPandaLib PRIVATE -fprofile-generate)
        target_compile_options(FatPanda PRIVATE -fprofile-generate)
        target_link_options(FatPanda PRIVATE -fprofile-generate)
    endif()
elseif(ENABLE_PGO STREQUAL "APPLY")
    message(STATUS "PGO: Profile Apply Mode enabled")
    if(MSVC)
        target_compile_options(FatPandaLib PRIVATE /GL)
        target_compile_options(FatPanda PRIVATE /GL)
        target_link_options(FatPanda PRIVATE /LTCG /USEPROFILE)
    else()
        target_compile_options(FatPandaLib PRIVATE -fprofile-use)
        target_compile_options(FatPanda PRIVATE -fprofile-use)
        target_link_options(FatPanda PRIVATE -fprofile-use)
    endif()
endif()

# Add test suite subdirectory
add_subdirectory(tests)
