#include "evaluation.hpp"
#include "nnue.hpp"
#include "board/attacks.hpp"
#include "board/types.hpp"
#include "params.hpp"

namespace ChessEngine {

namespace {

// Mirror square vertically for Black
inline Square get_black_square(Square sq) {
    return static_cast<Square>(static_cast<uint8_t>(sq) ^ 56);
}

constexpr Bitboard LIGHT_SQUARES = 0x55AA55AA55AA55AAULL;
constexpr Bitboard DARK_SQUARES  = 0xAA55AA55AA55AA55ULL;

// Generate bitboard masks representing files A-H
constexpr std::array<Bitboard, 8> FILE_MASKS = []() {
    std::array<Bitboard, 8> masks{};
    for (int file = 0; file < 8; ++file) {
        masks[file] = 0x0101010101010101ULL << file;
    }
    return masks;
}();

// Helper to compute a passed pawn mask
constexpr Bitboard get_passed_pawn_mask(Square sq, Color color) {
    int file = get_file(sq);
    int rank = get_rank(sq);
    Bitboard mask = EMPTY_BOARD;

    Bitboard files = FILE_MASKS[file];
    if (file > 0) files |= FILE_MASKS[file - 1];
    if (file < 7) files |= FILE_MASKS[file + 1];

    if (color == Color::White) {
        for (int r = rank + 1; r < 8; ++r) {
            mask |= (files & (0xFFULL << (r * 8)));
        }
    } else {
        for (int r = rank - 1; r >= 0; --r) {
            mask |= (files & (0xFFULL << (r * 8)));
        }
    }
    return mask;
}

} // namespace

// --- Modular Classical Evaluation Helpers ---

int evaluateMaterial(const Board& board) {
    int score = 0;

    // Piece Material Counts
    score += count_bits(board.get_piece_bitboard(Piece::WhitePawn)) * g_eval_params.val_pawn;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackPawn)) * g_eval_params.val_pawn;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteKnight)) * g_eval_params.val_knight;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackKnight)) * g_eval_params.val_knight;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) * g_eval_params.val_bishop;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackBishop)) * g_eval_params.val_bishop;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteRook)) * g_eval_params.val_rook;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackRook)) * g_eval_params.val_rook;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteQueen)) * g_eval_params.val_queen;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackQueen)) * g_eval_params.val_queen;

    // Bishop Pair Bonus
    if (count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) >= 2) score += 50;
    if (count_bits(board.get_piece_bitboard(Piece::BlackBishop)) >= 2) score -= 50;

    return score;
}

int evaluatePieceSquareTables(const Board& board, int eg_weight) {
    int score = 0;

    // White Pieces
    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    while (w_pawns) score += g_eval_params.pawn_pst[static_cast<size_t>(pop_lsb(w_pawns))];

    Bitboard w_knights = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights) score += g_eval_params.knight_pst[static_cast<size_t>(pop_lsb(w_knights))];

    Bitboard w_bishops = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops) score += g_eval_params.bishop_pst[static_cast<size_t>(pop_lsb(w_bishops))];

    Bitboard w_rooks = board.get_piece_bitboard(Piece::WhiteRook);
    while (w_rooks) score += g_eval_params.rook_pst[static_cast<size_t>(pop_lsb(w_rooks))];

    Bitboard w_queens = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens) score += g_eval_params.queen_pst[static_cast<size_t>(pop_lsb(w_queens))];

    Bitboard w_king = board.get_piece_bitboard(Piece::WhiteKing);
    if (w_king) {
        size_t sq = static_cast<size_t>(get_lsb(w_king));
        int mg_score = g_eval_params.king_pst[sq];
        int eg_score = g_eval_params.king_endgame_pst[sq];
        score += (mg_score * (256 - eg_weight) + eg_score * eg_weight) / 256;
    }

    // Black Pieces
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);
    while (b_pawns) score -= g_eval_params.pawn_pst[static_cast<size_t>(get_black_square(pop_lsb(b_pawns)))];

    Bitboard b_knights = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights) score -= g_eval_params.knight_pst[static_cast<size_t>(get_black_square(pop_lsb(b_knights)))];

    Bitboard b_bishops = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops) score -= g_eval_params.bishop_pst[static_cast<size_t>(get_black_square(pop_lsb(b_bishops)))];

    Bitboard b_rooks = board.get_piece_bitboard(Piece::BlackRook);
    while (b_rooks) score -= g_eval_params.rook_pst[static_cast<size_t>(get_black_square(pop_lsb(b_rooks)))];

    Bitboard b_queens = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens) score -= g_eval_params.queen_pst[static_cast<size_t>(get_black_square(pop_lsb(b_queens)))];

    Bitboard b_king = board.get_piece_bitboard(Piece::BlackKing);
    if (b_king) {
        size_t sq = static_cast<size_t>(get_black_square(get_lsb(b_king)));
        int mg_score = g_eval_params.king_pst[sq];
        int eg_score = g_eval_params.king_endgame_pst[sq];
        score -= (mg_score * (256 - eg_weight) + eg_score * eg_weight) / 256;
    }

    return score;
}

int evaluateMobility(const Board& board) {
    int score = 0;

    Bitboard white_occ = board.get_occupancy(Color::White);
    Bitboard black_occ = board.get_occupancy(Color::Black);
    Bitboard both_occ = board.get_occupancy(Color::None);

    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);

    // --- White Mobility & Piece Activity ---
    Bitboard w_knights = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights) {
        Square sq = pop_lsb(w_knights);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score += count_bits(get_knight_attacks(sq) & ~white_occ) * 4;

        // Knight Outpost: rank 3, 4, 5 (4th, 5th, 6th rank), supported by friendly pawn, cannot be attacked by enemy pawn
        if (rank >= 3 && rank <= 5) {
            bool supported_by_pawn = (get_pawn_attacks(sq, Color::Black) & w_pawns) != EMPTY_BOARD;
            if (supported_by_pawn) {
                Bitboard enemy_pawn_can_attack = EMPTY_BOARD;
                if (file > 0) {
                    enemy_pawn_can_attack |= (b_pawns & FILE_MASKS[file - 1] & ~((1ULL << ((rank + 1) * 8)) - 1));
                }
                if (file < 7) {
                    enemy_pawn_can_attack |= (b_pawns & FILE_MASKS[file + 1] & ~((1ULL << ((rank + 1) * 8)) - 1));
                }
                if (enemy_pawn_can_attack == EMPTY_BOARD) {
                    score += (file >= 2 && file <= 5) ? 25 : 15; // Central outposts are strongest
                }
            }
        }
    }

    Bitboard w_bishops = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops) {
        Square sq = pop_lsb(w_bishops);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score += count_bits(get_bishop_attacks(sq, both_occ) & ~white_occ) * 3;
        Bitboard same_color_pawns = (test_bit(LIGHT_SQUARES, sq)) ? (w_pawns & LIGHT_SQUARES) : (w_pawns & DARK_SQUARES);
        score -= static_cast<int>(count_bits(same_color_pawns)) * 3;

        // Bishop Outpost
        if (rank >= 3 && rank <= 5) {
            bool supported_by_pawn = (get_pawn_attacks(sq, Color::Black) & w_pawns) != EMPTY_BOARD;
            if (supported_by_pawn) {
                Bitboard enemy_pawn_can_attack = EMPTY_BOARD;
                if (file > 0) {
                    enemy_pawn_can_attack |= (b_pawns & FILE_MASKS[file - 1] & ~((1ULL << ((rank + 1) * 8)) - 1));
                }
                if (file < 7) {
                    enemy_pawn_can_attack |= (b_pawns & FILE_MASKS[file + 1] & ~((1ULL << ((rank + 1) * 8)) - 1));
                }
                if (enemy_pawn_can_attack == EMPTY_BOARD) {
                    score += 15;
                }
            }
        }

        // Long diagonal bonus (a1-h8 and h1-a8)
        constexpr Bitboard MAIN_DIAG_1 = 0x8040201008040201ULL;
        constexpr Bitboard MAIN_DIAG_2 = 0x0102040810204080ULL;
        if (test_bit(MAIN_DIAG_1 | MAIN_DIAG_2, sq)) {
            score += 8;
        }
    }

    Bitboard w_rooks = board.get_piece_bitboard(Piece::WhiteRook);
    Bitboard w_rooks_copy = w_rooks;
    while (w_rooks_copy) {
        Square sq = pop_lsb(w_rooks_copy);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score += count_bits(get_rook_attacks(sq, both_occ) & ~white_occ) * 2;

        // Rook on 7th rank
        if (rank == 6) score += 20;

        // Rook on open/semi-open file
        Bitboard pawns_on_file = (w_pawns | b_pawns) & FILE_MASKS[file];
        if (pawns_on_file == EMPTY_BOARD) {
            score += 20; // Open file
        } else if ((w_pawns & FILE_MASKS[file]) == EMPTY_BOARD) {
            score += 10; // Semi-open file
        }

        // Connected rooks (battery)
        if (count_bits(w_rooks) >= 2) {
            Bitboard other_rooks = w_rooks ^ (1ULL << static_cast<int>(sq));
            if (get_rook_attacks(sq, both_occ) & other_rooks) {
                score += 8; // Each connected rook gets +8 (total +16 for both)
            }
        }
    }

    Bitboard w_queens = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens) {
        Square sq = pop_lsb(w_queens);
        score += count_bits(get_queen_attacks(sq, both_occ) & ~white_occ) * 1;
    }

    // --- Black Mobility & Piece Activity ---
    Bitboard b_knights = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights) {
        Square sq = pop_lsb(b_knights);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score -= count_bits(get_knight_attacks(sq) & ~black_occ) * 4;

        // Knight Outpost: rank 2, 3, 4 (rank index 4, 3, 2 for Black), supported by friendly pawn, cannot be attacked by enemy pawn
        if (rank >= 2 && rank <= 4) {
            bool supported_by_pawn = (get_pawn_attacks(sq, Color::White) & b_pawns) != EMPTY_BOARD;
            if (supported_by_pawn) {
                Bitboard enemy_pawn_can_attack = EMPTY_BOARD;
                if (file > 0) {
                    enemy_pawn_can_attack |= (w_pawns & FILE_MASKS[file - 1] & ((1ULL << (rank * 8)) - 1));
                }
                if (file < 7) {
                    enemy_pawn_can_attack |= (w_pawns & FILE_MASKS[file + 1] & ((1ULL << (rank * 8)) - 1));
                }
                if (enemy_pawn_can_attack == EMPTY_BOARD) {
                    score -= (file >= 2 && file <= 5) ? 25 : 15;
                }
            }
        }
    }

    Bitboard b_bishops = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops) {
        Square sq = pop_lsb(b_bishops);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score -= count_bits(get_bishop_attacks(sq, both_occ) & ~black_occ) * 3;
        Bitboard same_color_pawns = (test_bit(LIGHT_SQUARES, sq)) ? (b_pawns & LIGHT_SQUARES) : (b_pawns & DARK_SQUARES);
        score += static_cast<int>(count_bits(same_color_pawns)) * 3;

        // Bishop Outpost
        if (rank >= 2 && rank <= 4) {
            bool supported_by_pawn = (get_pawn_attacks(sq, Color::White) & b_pawns) != EMPTY_BOARD;
            if (supported_by_pawn) {
                Bitboard enemy_pawn_can_attack = EMPTY_BOARD;
                if (file > 0) {
                    enemy_pawn_can_attack |= (w_pawns & FILE_MASKS[file - 1] & ((1ULL << (rank * 8)) - 1));
                }
                if (file < 7) {
                    enemy_pawn_can_attack |= (w_pawns & FILE_MASKS[file + 1] & ((1ULL << (rank * 8)) - 1));
                }
                if (enemy_pawn_can_attack == EMPTY_BOARD) {
                    score -= 15;
                }
            }
        }

        // Long diagonal bonus
        constexpr Bitboard MAIN_DIAG_1 = 0x8040201008040201ULL;
        constexpr Bitboard MAIN_DIAG_2 = 0x0102040810204080ULL;
        if (test_bit(MAIN_DIAG_1 | MAIN_DIAG_2, sq)) {
            score -= 8;
        }
    }

    Bitboard b_rooks = board.get_piece_bitboard(Piece::BlackRook);
    Bitboard b_rooks_copy = b_rooks;
    while (b_rooks_copy) {
        Square sq = pop_lsb(b_rooks_copy);
        int file = get_file(sq);
        int rank = get_rank(sq);

        score -= count_bits(get_rook_attacks(sq, both_occ) & ~black_occ) * 2;

        // Rook on 7th rank (rank 2 from White's view, i.e., rank index 1)
        if (rank == 1) score -= 20;

        // Rook on open/semi-open file
        Bitboard pawns_on_file = (w_pawns | b_pawns) & FILE_MASKS[file];
        if (pawns_on_file == EMPTY_BOARD) {
            score -= 20; // Open file
        } else if ((b_pawns & FILE_MASKS[file]) == EMPTY_BOARD) {
            score -= 10; // Semi-open file
        }

        // Connected rooks (battery)
        if (count_bits(b_rooks) >= 2) {
            Bitboard other_rooks = b_rooks ^ (1ULL << static_cast<int>(sq));
            if (get_rook_attacks(sq, both_occ) & other_rooks) {
                score -= 8;
            }
        }
    }

    Bitboard b_queens = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens) {
        Square sq = pop_lsb(b_queens);
        score -= count_bits(get_queen_attacks(sq, both_occ) & ~black_occ) * 1;
    }

    return score;
}

int evaluatePawnStructure(const Board& board, int eg_weight) {
    int score = 0;

    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);
    Bitboard both_occ = board.get_occupancy(Color::None);

    Bitboard w_king_bb = board.get_piece_bitboard(Piece::WhiteKing);
    Bitboard b_king_bb = board.get_piece_bitboard(Piece::BlackKing);
    bool has_kings = (w_king_bb != EMPTY_BOARD && b_king_bb != EMPTY_BOARD);
    Square w_ksq = has_kings ? get_lsb(w_king_bb) : Square::A1;
    Square b_ksq = has_kings ? get_lsb(b_king_bb) : Square::H8;

    constexpr int PASSED_BONUS[8] = { 0, 10, 15, 25, 45, 80, 140, 0 };

    // --- White Pawns ---
    Bitboard w_pawns_ref = w_pawns;
    while (w_pawns_ref) {
        Square sq = pop_lsb(w_pawns_ref);
        int file = get_file(sq);
        int rank = get_rank(sq);

        // Doubled Pawn
        Bitboard same_file_pawns = w_pawns & FILE_MASKS[file];
        if (count_bits(same_file_pawns) > 1) {
            score -= 15;
        }

        // Isolated Pawn
        Bitboard adjacent_files = (file > 0 ? FILE_MASKS[file - 1] : 0) | (file < 7 ? FILE_MASKS[file + 1] : 0);
        if ((adjacent_files & w_pawns) == EMPTY_BOARD) {
            score -= 20;
        }

        // Connected / Protected Pawn Bonus
        if (get_pawn_attacks(sq, Color::Black) & w_pawns) {
            score += 10;
        }

        // Phalanx Bonus (pawns side by side)
        if (file < 7 && test_bit(w_pawns, static_cast<Square>(static_cast<int>(sq) + 1))) {
            score += 8;
        }

        // Passed Pawn
        Bitboard passed_mask = get_passed_pawn_mask(sq, Color::White);
        if ((passed_mask & b_pawns) == EMPTY_BOARD) {
            int bonus = PASSED_BONUS[rank] * (256 + eg_weight) / 256;
            if (get_pawn_attacks(sq, Color::Black) & w_pawns) {
                bonus += 15;
            }
            if (rank < 7 && test_bit(board.get_occupancy(Color::Black), static_cast<Square>(static_cast<int>(sq) + 8))) {
                bonus = bonus * 6 / 10;
            }
            // Free unblocked path to promotion
            Bitboard front_squares = FILE_MASKS[file] & ~((1ULL << ((rank + 1) * 8)) - 1);
            if ((front_squares & both_occ) == EMPTY_BOARD) {
                bonus += (10 + rank * 4) * eg_weight / 256;
            }
            // King proximity in endgame
            if (has_kings) {
                int w_dist = std::max(std::abs(get_file(w_ksq) - file), std::abs(get_rank(w_ksq) - rank));
                int b_dist = std::max(std::abs(get_file(b_ksq) - file), std::abs(get_rank(b_ksq) - 7));
                bonus += (7 - w_dist) * 3 * eg_weight / 256;
                bonus += (b_dist - 2) * 3 * eg_weight / 256;
            }
            score += bonus;
        }
    }

    // --- Black Pawns ---
    Bitboard b_pawns_ref = b_pawns;
    while (b_pawns_ref) {
        Square sq = pop_lsb(b_pawns_ref);
        int file = get_file(sq);
        int rank = get_rank(sq);

        // Doubled Pawn
        Bitboard same_file_pawns = b_pawns & FILE_MASKS[file];
        if (count_bits(same_file_pawns) > 1) {
            score += 15;
        }

        // Isolated Pawn
        Bitboard adjacent_files = (file > 0 ? FILE_MASKS[file - 1] : 0) | (file < 7 ? FILE_MASKS[file + 1] : 0);
        if ((adjacent_files & b_pawns) == EMPTY_BOARD) {
            score -= 20;
        }

        // Connected / Protected Pawn Bonus
        if (get_pawn_attacks(sq, Color::White) & b_pawns) {
            score -= 10;
        }

        // Phalanx Bonus (pawns side by side)
        if (file < 7 && test_bit(b_pawns, static_cast<Square>(static_cast<int>(sq) + 1))) {
            score -= 8;
        }

        // Passed Pawn
        Bitboard passed_mask = get_passed_pawn_mask(sq, Color::Black);
        if ((passed_mask & w_pawns) == EMPTY_BOARD) {
            int bonus = PASSED_BONUS[7 - rank] * (256 + eg_weight) / 256;
            if (get_pawn_attacks(sq, Color::White) & b_pawns) {
                bonus += 15;
            }
            if (rank > 0 && test_bit(board.get_occupancy(Color::White), static_cast<Square>(static_cast<int>(sq) - 8))) {
                bonus = bonus * 6 / 10;
            }
            // Free unblocked path to promotion
            Bitboard front_squares = FILE_MASKS[file] & ((1ULL << (rank * 8)) - 1);
            if ((front_squares & both_occ) == EMPTY_BOARD) {
                bonus += (10 + (7 - rank) * 4) * eg_weight / 256;
            }
            // King proximity in endgame
            if (has_kings) {
                int b_dist = std::max(std::abs(get_file(b_ksq) - file), std::abs(get_rank(b_ksq) - rank));
                int w_dist = std::max(std::abs(get_file(w_ksq) - file), std::abs(get_rank(w_ksq) - 0));
                bonus += (7 - b_dist) * 3 * eg_weight / 256;
                bonus += (w_dist - 2) * 3 * eg_weight / 256;
            }
            score -= bonus;
        }
    }

    return score;
}

// Evaluate king safety for a given color (White or Black)
// Returns penalty as a positive integer (subtracted for White, added for Black)
inline int evaluate_side_king_safety(const Board& board, Color king_color, Bitboard both_occ) {
    Color enemy_color = (king_color == Color::White) ? Color::Black : Color::White;
    Piece king_piece = (king_color == Color::White) ? Piece::WhiteKing : Piece::BlackKing;
    Bitboard k_bb = board.get_piece_bitboard(king_piece);
    if (!k_bb) return 0;

    Square ksq = get_lsb(k_bb);
    int k_file = get_file(ksq);
    int k_rank = get_rank(ksq);

    int penalty = 0;

    Bitboard friendly_pawns = board.get_piece_bitboard(make_piece(king_color, PieceType::Pawn));
    Bitboard enemy_pawns = board.get_piece_bitboard(make_piece(enemy_color, PieceType::Pawn));
    Bitboard all_pawns = friendly_pawns | enemy_pawns;

    // 1. Pawn Shelter & File Openness near King
    int min_file = std::max(0, k_file - 1);
    int max_file = std::min(7, k_file + 1);

    for (int f = min_file; f <= max_file; ++f) {
        Bitboard file_mask = FILE_MASKS[f];
        // Fully open file towards king
        if ((all_pawns & file_mask) == EMPTY_BOARD) {
            penalty += (f == k_file) ? 25 : 15;
        }
        // Semi-open file (no friendly pawn shielding king)
        else if ((friendly_pawns & file_mask) == EMPTY_BOARD) {
            penalty += (f == k_file) ? 18 : 10;
        }
    }

    // Pawn shield check for castled kings
    if (king_color == Color::White && k_rank <= 1) {
        if (k_file >= 5) {
            if (board.get_piece(Square::F2) != Piece::WhitePawn) penalty += (board.get_piece(Square::F3) == Piece::WhitePawn ? 8 : 20);
            if (board.get_piece(Square::G2) != Piece::WhitePawn) penalty += (board.get_piece(Square::G3) == Piece::WhitePawn ? 10 : 22);
            if (board.get_piece(Square::H2) != Piece::WhitePawn) penalty += (board.get_piece(Square::H3) == Piece::WhitePawn ? 8 : 20);
        } else if (k_file <= 2) {
            if (board.get_piece(Square::A2) != Piece::WhitePawn) penalty += (board.get_piece(Square::A3) == Piece::WhitePawn ? 8 : 20);
            if (board.get_piece(Square::B2) != Piece::WhitePawn) penalty += (board.get_piece(Square::B3) == Piece::WhitePawn ? 10 : 22);
            if (board.get_piece(Square::C2) != Piece::WhitePawn) penalty += (board.get_piece(Square::C3) == Piece::WhitePawn ? 8 : 20);
        } else {
            // King stuck on center files
            if ((board.get_castling_rights() & (Castling::WK | Castling::WQ)) == 0) {
                penalty += 30;
            }
        }
    } else if (king_color == Color::Black && k_rank >= 6) {
        if (k_file >= 5) {
            if (board.get_piece(Square::F7) != Piece::BlackPawn) penalty += (board.get_piece(Square::F6) == Piece::BlackPawn ? 8 : 20);
            if (board.get_piece(Square::G7) != Piece::BlackPawn) penalty += (board.get_piece(Square::G6) == Piece::BlackPawn ? 10 : 22);
            if (board.get_piece(Square::H7) != Piece::BlackPawn) penalty += (board.get_piece(Square::H6) == Piece::BlackPawn ? 8 : 20);
        } else if (k_file <= 2) {
            if (board.get_piece(Square::A7) != Piece::BlackPawn) penalty += (board.get_piece(Square::A6) == Piece::BlackPawn ? 8 : 20);
            if (board.get_piece(Square::B7) != Piece::BlackPawn) penalty += (board.get_piece(Square::B6) == Piece::BlackPawn ? 10 : 22);
            if (board.get_piece(Square::C7) != Piece::BlackPawn) penalty += (board.get_piece(Square::C6) == Piece::BlackPawn ? 8 : 20);
        } else {
            // King stuck on center files
            if ((board.get_castling_rights() & (Castling::BK | Castling::BQ)) == 0) {
                penalty += 30;
            }
        }
    }

    // 2. King Attack Zone (Virtual King Ring)
    Bitboard king_ring = get_king_attacks(ksq);
    if (king_color == Color::White && k_rank < 7) {
        king_ring |= (king_ring << 8);
    } else if (king_color == Color::Black && k_rank > 0) {
        king_ring |= (king_ring >> 8);
    }

    int attack_units = 0;
    int attacker_count = 0;

    // Enemy Knights
    Bitboard enemy_knights = board.get_piece_bitboard(make_piece(enemy_color, PieceType::Knight));
    while (enemy_knights) {
        Square sq = pop_lsb(enemy_knights);
        Bitboard att = get_knight_attacks(sq) & king_ring;
        if (att) {
            attacker_count++;
            attack_units += 2 * static_cast<int>(count_bits(att));
        }
    }

    // Enemy Bishops
    Bitboard enemy_bishops = board.get_piece_bitboard(make_piece(enemy_color, PieceType::Bishop));
    while (enemy_bishops) {
        Square sq = pop_lsb(enemy_bishops);
        Bitboard att = get_bishop_attacks(sq, both_occ) & king_ring;
        if (att) {
            attacker_count++;
            attack_units += 2 * static_cast<int>(count_bits(att));
        }
    }

    // Enemy Rooks
    Bitboard enemy_rooks = board.get_piece_bitboard(make_piece(enemy_color, PieceType::Rook));
    while (enemy_rooks) {
        Square sq = pop_lsb(enemy_rooks);
        Bitboard att = get_rook_attacks(sq, both_occ) & king_ring;
        if (att) {
            attacker_count++;
            attack_units += 3 * static_cast<int>(count_bits(att));
        }
    }

    // Enemy Queens
    Bitboard enemy_queens = board.get_piece_bitboard(make_piece(enemy_color, PieceType::Queen));
    while (enemy_queens) {
        Square sq = pop_lsb(enemy_queens);
        Bitboard att = get_queen_attacks(sq, both_occ) & king_ring;
        if (att) {
            attacker_count++;
            attack_units += 5 * static_cast<int>(count_bits(att));
        }
    }

    if (attacker_count >= 2) {
        int danger = (attack_units * attack_units) / 6;
        penalty += std::min(150, danger);
    }

    return penalty;
}

int evaluateKingSafety(const Board& board, int eg_weight) {
    Bitboard both_occ = board.get_occupancy(Color::None);
    int white_penalty = evaluate_side_king_safety(board, Color::White, both_occ);
    int black_penalty = evaluate_side_king_safety(board, Color::Black, both_occ);

    int net_safety = black_penalty - white_penalty;
    // Scale king safety score down as we enter endgame
    return net_safety * (256 - eg_weight) / 256;
}

int get_game_phase(const Board& board) {
    int npm = 0;
    npm += count_bits(board.get_piece_bitboard(Piece::WhiteKnight)) * g_eval_params.val_knight;
    npm += count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) * g_eval_params.val_bishop;
    npm += count_bits(board.get_piece_bitboard(Piece::WhiteRook)) * g_eval_params.val_rook;
    npm += count_bits(board.get_piece_bitboard(Piece::WhiteQueen)) * g_eval_params.val_queen;
    npm += count_bits(board.get_piece_bitboard(Piece::BlackKnight)) * g_eval_params.val_knight;
    npm += count_bits(board.get_piece_bitboard(Piece::BlackBishop)) * g_eval_params.val_bishop;
    npm += count_bits(board.get_piece_bitboard(Piece::BlackRook)) * g_eval_params.val_rook;
    npm += count_bits(board.get_piece_bitboard(Piece::BlackQueen)) * g_eval_params.val_queen;
    return npm;
}

int evaluateEndgameHeuristics(const Board& board) {
    // Check if there are no pawns for both sides
    if (board.get_piece_bitboard(Piece::WhitePawn) || board.get_piece_bitboard(Piece::BlackPawn)) {
        return 0;
    }

    int score = 0;
    
    // We only apply mop-up if one side has winning material advantage and the other has only a King.
    int w_npm = count_bits(board.get_piece_bitboard(Piece::WhiteKnight)) * g_eval_params.val_knight
              + count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) * g_eval_params.val_bishop
              + count_bits(board.get_piece_bitboard(Piece::WhiteRook)) * g_eval_params.val_rook
              + count_bits(board.get_piece_bitboard(Piece::WhiteQueen)) * g_eval_params.val_queen;
              
    int b_npm = count_bits(board.get_piece_bitboard(Piece::BlackKnight)) * g_eval_params.val_knight
              + count_bits(board.get_piece_bitboard(Piece::BlackBishop)) * g_eval_params.val_bishop
              + count_bits(board.get_piece_bitboard(Piece::BlackRook)) * g_eval_params.val_rook
              + count_bits(board.get_piece_bitboard(Piece::BlackQueen)) * g_eval_params.val_queen;
              
    Square w_ksq = get_lsb(board.get_piece_bitboard(Piece::WhiteKing));
    Square b_ksq = get_lsb(board.get_piece_bitboard(Piece::BlackKing));
    
    if (w_npm > 0 && b_npm == 0) {
        // White has winning advantage, Black has only King.
        // Drive Black King to corner/edge and White King closer.
        int dist = std::max(std::abs(get_file(w_ksq) - get_file(b_ksq)), std::abs(get_rank(w_ksq) - get_rank(b_ksq)));
        score += (8 - dist) * 10;
        
        int b_file = get_file(b_ksq);
        int b_rank = get_rank(b_ksq);
        int file_dist = std::max(0, std::abs(2 * b_file - 7) - 1) / 2;
        int rank_dist = std::max(0, std::abs(2 * b_rank - 7) - 1) / 2;
        int center_dist = std::max(file_dist, rank_dist);
        score += center_dist * 20;
    } else if (b_npm > 0 && w_npm == 0) {
        // Black has winning advantage, White has only King.
        // Drive White King to corner/edge and Black King closer.
        int dist = std::max(std::abs(get_file(w_ksq) - get_file(b_ksq)), std::abs(get_rank(w_ksq) - get_rank(b_ksq)));
        score -= (8 - dist) * 10;
        
        int w_file = get_file(w_ksq);
        int w_rank = get_rank(w_ksq);
        int file_dist = std::max(0, std::abs(2 * w_file - 7) - 1) / 2;
        int rank_dist = std::max(0, std::abs(2 * w_rank - 7) - 1) / 2;
        int center_dist = std::max(file_dist, rank_dist);
        score -= center_dist * 20;
    }
    
    return score;
}

// Combine all elements to return side-to-move perspective score
int evaluate_classical(const Board& board) {
    int npm = get_game_phase(board);
    // eg_weight scales from 0 (Middlegame) to 256 (Endgame)
    int eg_weight = 256 - (std::min(6000, npm) * 256 / 6000);

    int score = evaluateMaterial(board)
              + evaluatePieceSquareTables(board, eg_weight)
              + evaluateMobility(board)
              + evaluatePawnStructure(board, eg_weight)
              + evaluateKingSafety(board, eg_weight)
              + evaluateEndgameHeuristics(board);

    return (board.get_side_to_move() == Color::White) ? score : -score;
}

int evaluate(const Board& board) {
    if (g_use_nnue) {
        return nnue_evaluate(board);
    }
    return evaluate_classical(board);
}

} // namespace ChessEngine
