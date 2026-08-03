#pragma once

#include "board.hpp"
#include "move/move.hpp"
#include <string>

namespace ChessEngine {

// Compute the standard Polyglot Zobrist hash key for the given board position.
uint64_t compute_polyglot_hash(const Board& board);

// Lookup a move in the Polyglot opening book file.
// Returns MOVE_NONE if out of book or if the file cannot be opened.
Move lookup_book_move(const Board& board, const std::string& book_path);

} // namespace ChessEngine
