#pragma once

#include "board/board.hpp"

namespace ChessEngine {

// Static evaluation function returning the score of a position in centipawns
// from the side-to-move's perspective
int evaluate(const Board& board);

// Modular components of the evaluation function
int evaluateMaterial(const Board& board);
int evaluatePieceSquareTables(const Board& board);
int evaluateMobility(const Board& board);
int evaluatePawnStructure(const Board& board);
int evaluateKingSafety(const Board& board);

} // namespace ChessEngine
