#include "polyglot.hpp"
#include "polyglot_constants.hpp"
#include "board.hpp"
#include "movegen.hpp"
#include "utils/rng.hpp"
#include <fstream>
#include <random>
#include <vector>
#include <algorithm>

namespace ChessEngine {

// Platform-independent big-endian to host byte swaps
static inline uint16_t swap_16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint64_t swap_64(uint64_t val) {
    return ((val >> 56) & 0x00000000000000FFULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 56) & 0xFF00000000000000ULL);
}

// Helper to convert internal square value to Polyglot index
static inline int to_polyglot_square(Square sq) {
    int val = static_cast<int>(sq);
    int file = val % 8;
    int rank = val / 8;
    return 8 * (7 - rank) + file;
}

// Helper to convert Polyglot index back to internal square
static inline Square from_polyglot_square(int poly_sq) {
    int file = poly_sq % 8;
    int rank = poly_sq / 8;
    int val = 8 * (7 - rank) + file;
    return static_cast<Square>(val);
}

// Map piece to Polyglot index
static inline int to_polyglot_piece(Piece pc) {
    switch (pc) {
        case Piece::WhitePawn:   return 0;
        case Piece::BlackPawn:   return 1;
        case Piece::WhiteKnight: return 2;
        case Piece::BlackKnight: return 3;
        case Piece::WhiteBishop: return 4;
        case Piece::BlackBishop: return 5;
        case Piece::WhiteRook:   return 6;
        case Piece::BlackRook:   return 7;
        case Piece::WhiteQueen:  return 8;
        case Piece::BlackQueen:  return 9;
        case Piece::WhiteKing:   return 10;
        case Piece::BlackKing:   return 11;
        default: return -1;
    }
}

uint64_t compute_polyglot_hash(const Board& board) {
    uint64_t hash = 0;

    // 1. Pieces
    for (int sq_val = 0; sq_val < 64; ++sq_val) {
        Square sq = static_cast<Square>(sq_val);
        Piece pc = board.get_piece(sq);
        if (pc != Piece::None) {
            int p_idx = to_polyglot_piece(pc);
            if (p_idx != -1) {
                int poly_sq = to_polyglot_square(sq);
                hash ^= POLYGLOT_RANDOM_ARRAY[p_idx * 64 + poly_sq];
            }
        }
    }

    // 2. Castling Rights
    uint8_t rights = board.get_castling_rights();
    if (rights & Castling::WK) hash ^= POLYGLOT_RANDOM_ARRAY[768];
    if (rights & Castling::WQ) hash ^= POLYGLOT_RANDOM_ARRAY[768 + 1];
    if (rights & Castling::BK) hash ^= POLYGLOT_RANDOM_ARRAY[768 + 2];
    if (rights & Castling::BQ) hash ^= POLYGLOT_RANDOM_ARRAY[768 + 3];

    // 3. En Passant Square
    // Only hashed if there is a pawn of the side-to-move ready to capture the EP square.
    Square ep_sq = board.get_en_passant();
    if (ep_sq != Square::None) {
        Color turn = board.get_side_to_move();
        int ep_file = static_cast<int>(ep_sq) % 8;
        
        bool ep_hashable = false;
        
        if (turn == Color::White) {
            // White to move, so Black just moved double-step.
            // EP target square is on Rank 6 (index 5).
            // White pawns that can capture must be on Rank 5 (index 4).
            if (ep_file > 0) {
                Square left_pawn_sq = static_cast<Square>(4 * 8 + (ep_file - 1));
                if (board.get_piece(left_pawn_sq) == Piece::WhitePawn) {
                    ep_hashable = true;
                }
            }
            if (ep_file < 7) {
                Square right_pawn_sq = static_cast<Square>(4 * 8 + (ep_file + 1));
                if (board.get_piece(right_pawn_sq) == Piece::WhitePawn) {
                    ep_hashable = true;
                }
            }
        } else {
            // Black to move, so White just moved double-step.
            // EP target square is on Rank 3 (index 2).
            // Black pawns that can capture must be on Rank 4 (index 3).
            if (ep_file > 0) {
                Square left_pawn_sq = static_cast<Square>(3 * 8 + (ep_file - 1));
                if (board.get_piece(left_pawn_sq) == Piece::BlackPawn) {
                    ep_hashable = true;
                }
            }
            if (ep_file < 7) {
                Square right_pawn_sq = static_cast<Square>(3 * 8 + (ep_file + 1));
                if (board.get_piece(right_pawn_sq) == Piece::BlackPawn) {
                    ep_hashable = true;
                }
            }
        }

        if (ep_hashable) {
            hash ^= POLYGLOT_RANDOM_ARRAY[772 + ep_file];
        }
    }

    // 4. Side to Move
    // Hash in turn if it is White's turn
    if (board.get_side_to_move() == Color::White) {
        hash ^= POLYGLOT_RANDOM_ARRAY[780];
    }

    return hash;
}

// Polyglot entry layout
#pragma pack(push, 1)
struct PolyglotEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};
#pragma pack(pop)

Move lookup_book_move(const Board& board, const std::string& book_path) {
    std::ifstream file(book_path, std::ios::binary);
    if (!file) {
        return MOVE_NONE;
    }

    // Determine the size of the book in entries
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    if (file_size % 16 != 0 || file_size == 0) {
        return MOVE_NONE;
    }
    std::streamsize num_entries = file_size / 16;

    uint64_t target_key = compute_polyglot_hash(board);

    // Binary search on disk
    std::streamsize low = 0;
    std::streamsize high = num_entries - 1;
    std::streamsize match_index = -1;

    while (low <= high) {
        std::streamsize mid = low + (high - low) / 2;
        file.seekg(mid * 16, std::ios::beg);
        PolyglotEntry entry;
        file.read(reinterpret_cast<char*>(&entry), 16);

        uint64_t entry_key = swap_64(entry.key);

        if (entry_key == target_key) {
            match_index = mid;
            break;
        } else if (entry_key < target_key) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (match_index == -1) {
        return MOVE_NONE;
    }

    // Find the first matching entry
    std::streamsize first_match = match_index;
    while (first_match > 0) {
        file.seekg((first_match - 1) * 16, std::ios::beg);
        PolyglotEntry entry;
        file.read(reinterpret_cast<char*>(&entry), 16);
        if (swap_64(entry.key) == target_key) {
            first_match--;
        } else {
            break;
        }
    }

    // Read all contiguous matching entries
    std::vector<std::pair<Move, int>> candidate_moves;
    Board temp_board = board;
    std::vector<Move> legal_moves = generate_legal_moves(temp_board);

    file.seekg(first_match * 16, std::ios::beg);
    while (true) {
        PolyglotEntry entry;
        if (!file.read(reinterpret_cast<char*>(&entry), 16)) {
            break;
        }
        if (swap_64(entry.key) != target_key) {
            break;
        }

        uint16_t move_code = swap_16(entry.move);
        uint16_t weight = swap_16(entry.weight);

        // Decode Polyglot move
        int to_poly_sq = move_code & 0x3F;
        int from_poly_sq = (move_code >> 6) & 0x3F;
        int promo_code = (move_code >> 12) & 7;

        Square from_sq = from_poly_sq == 0 && to_poly_sq == 0 ? Square::None : from_polyglot_square(from_poly_sq);
        Square to_sq = from_poly_sq == 0 && to_poly_sq == 0 ? Square::None : from_polyglot_square(to_poly_sq);

        PieceType promo_type = PieceType::None;
        if (promo_code == 1) promo_type = PieceType::Knight;
        else if (promo_code == 2) promo_type = PieceType::Bishop;
        else if (promo_code == 3) promo_type = PieceType::Rook;
        else if (promo_code == 4) promo_type = PieceType::Queen;

        // Match against legal moves
        Move matched_move = MOVE_NONE;
        for (const auto& m : legal_moves) {
            if (m.get_from() == from_sq && m.get_promotion_piece_type() == promo_type) {
                if (m.get_to() == to_sq) {
                    matched_move = m;
                    break;
                }
                if (m.is_castle()) {
                    int rank = get_rank(from_sq);
                    if (m.is_castle_k() && to_sq == make_square(6, rank)) {
                        matched_move = m;
                        break;
                    }
                    if (m.is_castle_q() && to_sq == make_square(2, rank)) {
                        matched_move = m;
                        break;
                    }
                }
            }
        }

        if (matched_move != MOVE_NONE) {
            candidate_moves.push_back({matched_move, weight});
        }
    }

    if (candidate_moves.empty()) {
        return MOVE_NONE;
    }

    // Select among candidate moves using weights
    uint32_t total_weight = 0;
    for (const auto& cand : candidate_moves) {
        total_weight += cand.second;
    }

    if (total_weight > 0) {
        uint32_t target = rand_range(0, total_weight - 1);

        uint32_t running_sum = 0;
        for (const auto& cand : candidate_moves) {
            running_sum += cand.second;
            if (running_sum > target) {
                return cand.first;
            }
        }
    }

    // Fallback if total_weight is 0
    return candidate_moves[rand_index(candidate_moves.size())].first;
}

} // namespace ChessEngine
