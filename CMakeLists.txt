cmake_minimum_required(VERSION 3.14)
project(ChessEngine VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing globally
enable_testing()

# Define the modular Chess Engine Library
add_library(ChessEngineLib STATIC
    src/utils/version.cpp
    src/board/board.cpp
    src/board/fen.cpp
    src/move/move.cpp
)

# Set include directories
target_include_directories(ChessEngineLib PUBLIC src)

# Set warnings for ChessEngineLib target
if(MSVC)
    target_compile_options(ChessEngineLib PRIVATE /W4 /permissive-)
else()
    target_compile_options(ChessEngineLib PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Define the Main Executable
add_executable(ChessEngine src/main.cpp)
target_link_libraries(ChessEngine PRIVATE ChessEngineLib)

# Set warnings for ChessEngine executable target
if(MSVC)
    target_compile_options(ChessEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(ChessEngine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Add test suite subdirectory
add_subdirectory(tests)
