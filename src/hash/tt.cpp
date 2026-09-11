#include "tt.hpp"
#include "search/search.hpp"
#include <cmath>
#include <algorithm>
#include <immintrin.h>

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
    // Each cluster is 64 bytes (4 entries of 16 bytes each).
    // 1 MB = 1,048,576 bytes.
    size_t num_clusters = (size_in_mb * 1024 * 1024) / sizeof(TTCluster);
    if (num_clusters == 0) num_clusters = 1;

    num_clusters_ = num_clusters;
    table_.clear();
    table_.resize(num_clusters);
}

void TranspositionTable::clear() {
    for (auto& cluster : table_) {
        for (auto& entry : cluster.entries) {
            entry.key = 0;
            entry.move = MOVE_NONE;
            entry.score = 0;
            entry.depth = -1;
            entry.flags = 0;
            entry.age = 0;
        }
    }
}

void TranspositionTable::prefetch(uint64_t key) const {
    if (num_clusters_ == 0) return;
    size_t index = static_cast<size_t>(key % num_clusters_);
    _mm_prefetch(reinterpret_cast<const char*>(&table_[index]), _MM_HINT_T0);
}

bool TranspositionTable::probe(uint64_t key, int ply, TTEntry& entry) const {
    if (num_clusters_ == 0) return false;

    size_t index = static_cast<size_t>(key % num_clusters_);
    const TTCluster& cluster = table_[index];

    for (size_t i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].key == key) {
            entry = cluster.entries[i];

            // Adjust mate score back to ply-specific values
            if (entry.score > MATE_SCORE - MAX_PLY) {
                entry.score = static_cast<int16_t>(entry.score - ply);
            } else if (entry.score < -MATE_SCORE + MAX_PLY) {
                entry.score = static_cast<int16_t>(entry.score + ply);
            }
            return true;
        }
    }

    return false;
}

void TranspositionTable::record(uint64_t key, Move move, int score, int depth, uint8_t flags, int ply) {
    if (num_clusters_ == 0) return;

    // Adjust mate score to be path-length independent
    if (score > MATE_SCORE - MAX_PLY) {
        score += ply;
    } else if (score < -MATE_SCORE + MAX_PLY) {
        score -= ply;
    }

    size_t index = static_cast<size_t>(key % num_clusters_);
    TTCluster& cluster = table_[index];

    // Check if key already exists in cluster
    for (size_t i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].key == key) {
            if (move != MOVE_NONE || cluster.entries[i].move == MOVE_NONE) {
                cluster.entries[i].move = move;
            }
            if (flags == TT_EXACT || depth >= cluster.entries[i].depth - 2 || cluster.entries[i].age != age_) {
                cluster.entries[i].score = static_cast<int16_t>(score);
                cluster.entries[i].depth = static_cast<int8_t>(depth);
                cluster.entries[i].flags = flags;
                cluster.entries[i].age = age_;
            }
            return;
        }
    }

    // Check for an empty slot in cluster
    for (size_t i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].key == 0) {
            cluster.entries[i].key = key;
            cluster.entries[i].move = move;
            cluster.entries[i].score = static_cast<int16_t>(score);
            cluster.entries[i].depth = static_cast<int8_t>(depth);
            cluster.entries[i].flags = flags;
            cluster.entries[i].age = age_;
            return;
        }
    }

    // Replace the slot with lowest utility (age-penalized depth)
    size_t replace_idx = 0;
    int min_utility = 1000000;
    for (size_t i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        int age_diff = static_cast<int>(static_cast<uint8_t>(age_ - cluster.entries[i].age));
        int utility = static_cast<int>(cluster.entries[i].depth) - age_diff * 4;
        if (utility < min_utility) {
            min_utility = utility;
            replace_idx = i;
        }
    }

    cluster.entries[replace_idx].key = key;
    cluster.entries[replace_idx].move = move;
    cluster.entries[replace_idx].score = static_cast<int16_t>(score);
    cluster.entries[replace_idx].depth = static_cast<int8_t>(depth);
    cluster.entries[replace_idx].flags = flags;
    cluster.entries[replace_idx].age = age_;
}

} // namespace ChessEngine
