#include "tt.hpp"
#include "search/search.hpp"
#include <cmath>
#include <algorithm>

namespace ChessEngine {

// Define the global instance (default size 64MB)
TranspositionTable g_tt(64);

TranspositionTable::TranspositionTable() {
    resize(64);
}

TranspositionTable::TranspositionTable(size_t size_in_mb) {
    resize(size_in_mb);
}

void TranspositionTable::resize(size_t size_in_mb) {
    // Each entry is 16 bytes.
    // 1 MB = 1,048,576 bytes.
    size_t num_entries = (size_in_mb * 1024 * 1024) / sizeof(TTEntry);
    
    if (num_entries == 0) num_entries = 1;
    
    table_.clear();
    table_.resize(num_entries);
}

void TranspositionTable::clear() {
    for (auto& entry : table_) {
        entry.key = 0;
        entry.move = MOVE_NONE;
        entry.score = 0;
        entry.depth = -1;
        entry.flags = 0;
    }
}

bool TranspositionTable::probe(uint64_t key, int ply, TTEntry& entry) const {
    if (table_.empty()) return false;
    
    size_t index = static_cast<size_t>(key % table_.size());
    const TTEntry& table_entry = table_[index];
    
    if (table_entry.key == key) {
        entry = table_entry;
        
        // Adjust mate score back to ply-specific values
        if (entry.score > MATE_SCORE - MAX_PLY) {
            entry.score = static_cast<int16_t>(entry.score - ply);
        } else if (entry.score < -MATE_SCORE + MAX_PLY) {
            entry.score = static_cast<int16_t>(entry.score + ply);
        }
        return true;
    }
    
    return false;
}

void TranspositionTable::record(uint64_t key, Move move, int score, int depth, uint8_t flags, int ply) {
    if (table_.empty()) return;
    
    size_t index = static_cast<size_t>(key % table_.size());
    TTEntry& table_entry = table_[index];
    
    // Adjust mate score to be path-length independent
    if (score > MATE_SCORE - MAX_PLY) {
        score += ply;
    } else if (score < -MATE_SCORE + MAX_PLY) {
        score -= ply;
    }
    
    // Replacement strategy: write if empty slot, or if new depth >= old depth,
    // or if the entry is from a different position.
    if (table_entry.key != key || depth >= table_entry.depth) {
        table_entry.key = key;
        // Keep the previous best move if the new recording doesn't provide one
        if (move != MOVE_NONE || table_entry.key != key) {
            table_entry.move = move;
        }
        table_entry.score = static_cast<int16_t>(score);
        table_entry.depth = static_cast<int8_t>(depth);
        table_entry.flags = flags;
    }
}

} // namespace ChessEngine
