#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "move/move.hpp"

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
    uint8_t age = 0;        // Generation counter for age-based replacement
    uint8_t padding = 0;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

struct alignas(64) TTCluster {
    static constexpr size_t CLUSTER_SIZE = 4;
    std::array<TTEntry, CLUSTER_SIZE> entries{};
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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

    // Hardware prefetch instruction for the TT cluster
    void prefetch(uint64_t key) const;

    // Advance search generation age
    void new_search() { age_++; }

    // Get the total number of entries
    size_t get_entry_count() const { return num_clusters_ * TTCluster::CLUSTER_SIZE; }

private:
    std::vector<TTCluster> table_;
    size_t num_clusters_ = 0;
    uint8_t age_ = 0;
};

// Global transposition table instance
extern TranspositionTable g_tt;

} // namespace ChessEngine
