#pragma once
#include "board/board.hpp"
#include <string>
#include <sstream>

namespace ChessEngine {

extern int g_search_overhead_ms;
extern bool g_own_book;
extern std::string g_book_file;

// Start the Universal Chess Interface (UCI) protocol communication loop.
// Listens to commands from standard input and responds to standard output.
void uci_loop();

// Helper parsing functions exposed for testing
Move parse_move(Board& board, const std::string& move_str);
void parse_position(Board& board, std::stringstream& ss);
void parse_go(Board& board, std::stringstream& ss);
void join_search_thread();
void parse_tune(std::stringstream& ss);
void parse_datagen(std::stringstream& ss);
void parse_bench(std::stringstream& ss);
void run_benchmark(int depth = 13, int threads = 1, int hash_mb = 16);

} // namespace ChessEngine
