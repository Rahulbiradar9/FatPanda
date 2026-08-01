#include "perft.hpp"
#include "movegen.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace ChessEngine {

uint64_t runPerft(Board& board, int depth) {
    if (depth <= 0) return 1ULL;

    std::vector<Move> moves = generateLegalMoves(board);
    if (depth == 1) return moves.size();

    uint64_t total_nodes = 0;
    for (Move m : moves) {
        UndoState undo;
        if (board.makeMove(m, undo)) {
            total_nodes += runPerft(board, depth - 1);
            board.unmakeMove(m, undo);
        }
    }
    return total_nodes;
}

std::vector<std::pair<std::string, uint64_t>> runPerftDivide(Board& board, int depth, bool print_to_stdout) {
    std::vector<std::pair<std::string, uint64_t>> divide_results;
    if (depth <= 0) return divide_results;

    std::vector<Move> moves = generateLegalMoves(board);
    uint64_t total_nodes = 0;

    for (Move m : moves) {
        UndoState undo;
        if (board.makeMove(m, undo)) {
            uint64_t nodes = runPerft(board, depth - 1);
            total_nodes += nodes;
            board.unmakeMove(m, undo);
            
            std::string move_str = m.toString();
            divide_results.push_back({move_str, nodes});
            if (print_to_stdout) {
                std::cout << move_str << ": " << nodes << "\n";
            }
        }
    }

    if (print_to_stdout) {
        std::cout << "\nTotal nodes: " << total_nodes << "\n";
    }

    return divide_results;
}

PerftResult runPerftBenchmark(Board& board, int depth) {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t nodes = runPerft(board, depth);
    auto end = std::chrono::high_resolution_clock::now();

    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double nps = 0.0;
    if (time_ms > 0) {
        nps = (nodes / (time_ms / 1000.0));
    }

    return {nodes, time_ms, nps};
}

} // namespace ChessEngine
