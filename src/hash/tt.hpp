#pragma once

#include <cstdint>
#include <vector>
#include "move/move.hpp"

#include <mutex>
#include <array>

namespace ChessEngine {

// TT Entry flag types
constexpr uint8_t TT_EXACT = 0; // Exact evaluation score
constexpr uint8_t TT_ALPHA = 1; // Upper bound (fail-low, score <= alpha)
constexpr uint8_t TT_BETA  = 2; // Lower bound (fail-high, score >= beta)

struct TTEntry {
    uint64_t key = 0;       // Zobrist hash key
    Move move = MOVE_NONE;  // Best move from this position
    int16_t score = 0;      // Evaluation score (adjusted for mate)
    int8_t depth = -1;      // Depth of search
    uint8_t flags = 0;      // TT flags (EXACT, ALPHA, BETA)
};

class TranspositionTable {
public:
    TranspositionTable();
    explicit TranspositionTable(size_t size_in_mb);

    // Resize the transposition table to a given size in Megabytes
    void resize(size_t size_in_mb);

    // Clear all entries in the table
    void clear();

    // Probe the table for an entry. Adjusts mate scores using the current ply.
    // Returns true if a hit is found.
    bool probe(uint64_t key, int ply, TTEntry& entry) const;

    // Record a new entry in the table. Adjusts mate scores using the current ply.
    void record(uint64_t key, Move move, int score, int depth, uint8_t flags, int ply);

    // Get the total number of entries
    size_t get_entry_count() const { return table_.size(); }

private:
    std::vector<TTEntry> table_;
    mutable std::array<std::mutex, 4096> locks_;
};

// Global transposition table instance
extern TranspositionTable g_tt;

} // namespace ChessEngine
