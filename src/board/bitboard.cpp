#include "bitboard.hpp"
#include <iostream>

namespace ChessEngine {

void printBitboard(Bitboard bb) {
    std::cout << "\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            int sq = rank * 8 + file;
            if ((bb & (1ULL << sq)) != 0) {
                std::cout << "1 ";
            } else {
                std::cout << ". ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "  +-----------------\n";
    std::cout << "    a b c d e f g h\n\n";
}

} // namespace ChessEngine
