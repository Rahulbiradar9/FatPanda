#pragma once

#include "board/board.hpp"

namespace ChessEngine {

// Static evaluation function returning the score of a position in centipawns
// from the side-to-move's perspective
int evaluate(const Board& board);

// Classical evaluation function (handcrafted parameters)
int evaluate_classical(const Board& board);

int evaluateMaterial(const Board& board);
int evaluatePieceSquareTables(const Board& board, int eg_weight);
int evaluateMobility(const Board& board);
int evaluatePawnStructure(const Board& board, int eg_weight = 128);
int evaluateKingSafety(const Board& board, int eg_weight);
int evaluateEndgameHeuristics(const Board& board);

} // namespace ChessEngine
