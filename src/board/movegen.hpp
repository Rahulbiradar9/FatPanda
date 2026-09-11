#pragma once

#include <vector>
#include <array>
#include "board.hpp"

namespace ChessEngine {

struct MoveList {
    std::array<Move, 256> moves{};
    std::array<int, 256> scores{};
    size_t count = 0;

    inline void push_back(Move m, int score = 0) {
        if (count < 256) {
            moves[count] = m;
            scores[count] = score;
            count++;
        }
    }

    inline void clear() { count = 0; }
    inline size_t size() const { return count; }
    inline bool empty() const { return count == 0; }
    inline Move operator[](size_t idx) const { return moves[idx]; }
    inline Move& operator[](size_t idx) { return moves[idx]; }
};

// Determine whether a square is attacked by any piece of the attacker's color
bool is_square_attacked(const Board& board, Square sq, Color attacker);

// Determine whether a side's King is in check
bool is_in_check(const Board& board, Color color);

// Generate pseudo-legal moves into MoveList (Zero-allocation)
void generatePseudoLegalMoves(const Board& board, MoveList& moves);
void generatePseudoLegalCaptures(const Board& board, MoveList& moves);
void generatePseudoLegalQuiets(const Board& board, MoveList& moves);

// Generate all pseudo-legal moves (ignoring checks) - New camelCase
std::vector<Move> generatePseudoLegalMoves(const Board& board);

// Generate all pseudo-legal moves (ignoring checks) - Legacy compatibility
std::vector<Move> generate_pseudo_legal_moves(const Board& board);

// Generate all fully legal moves (ensuring own King is not left in check) - New camelCase
std::vector<Move> generateLegalMoves(Board& board);

// Generate all fully legal moves (ensuring own King is not left in check) - Legacy compatibility
std::vector<Move> generate_legal_moves(Board& board);

} // namespace ChessEngine

