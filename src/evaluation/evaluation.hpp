#pragma once

#include "board/board.hpp"

namespace ChessEngine {

// Static evaluation function returning the score of a position in centipawns
// from White's perspective (White winning is positive, Black is negative)
int evaluate(const Board& board);

} // namespace ChessEngine
