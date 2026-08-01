#pragma once

#include <vector>
#include "board.hpp"

namespace ChessEngine {

// Determine whether a square is attacked by any piece of the attacker's color
bool is_square_attacked(const Board& board, Square sq, Color attacker);

// Determine whether a side's King is in check
bool is_in_check(const Board& board, Color color);

// Generate all pseudo-legal moves (ignoring checks) - New camelCase
std::vector<Move> generatePseudoLegalMoves(const Board& board);

// Generate all pseudo-legal moves (ignoring checks) - Legacy compatibility
std::vector<Move> generate_pseudo_legal_moves(const Board& board);

// Generate all fully legal moves (ensuring own King is not left in check)
std::vector<Move> generate_legal_moves(Board& board);

} // namespace ChessEngine
