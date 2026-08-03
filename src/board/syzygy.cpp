#include "syzygy.hpp"
#include <iostream>

namespace ChessEngine {

bool g_syzygy_enabled = false;
std::string g_syzygy_path = "";
static bool s_syzygy_loaded = false;

void syzygy_init(const std::string& path) {
    g_syzygy_path = path;
    if (path.empty() || path == "<empty>") {
        s_syzygy_loaded = false;
        return;
    }
    
    // Stub prober initialization
    // In a real engine integration, we would invoke Fathom's tb_init(path.c_str()) here.
    // Since Syzygy is optional and external, we log a status message and set loaded flag.
    std::cout << "info string Syzygy tablebase path set to: " << path << std::endl;
    s_syzygy_loaded = true;
}

bool syzygy_is_loaded() {
    return s_syzygy_loaded && g_syzygy_enabled;
}

bool syzygy_probe_wdl(const Board& board, int& score) {
    (void)board;
    (void)score;
    // Returns false for tablebase miss.
    // If Fathom is integrated, we would probe using tb_probe_wdl(board) and return matching scores.
    return false;
}

bool syzygy_probe_root(const Board& board, Move& best_move) {
    (void)board;
    (void)best_move;
    // Returns false for tablebase miss.
    // If Fathom is integrated, we would probe using tb_probe_root(board) and select the best move.
    return false;
}

} // namespace ChessEngine
