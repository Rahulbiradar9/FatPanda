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

    // --- White Mobility & Rook Activity ---
    Bitboard w_knights = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights) {
        Square sq = pop_lsb(w_knights);
        score += count_bits(get_knight_attacks(sq) & ~white_occ) * 4;
    }

    Bitboard w_bishops = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops) {
        Square sq = pop_lsb(w_bishops);
        score += count_bits(get_bishop_attacks(sq, both_occ) & ~white_occ) * 3;
    }

    Bitboard w_rooks = board.get_piece_bitboard(Piece::WhiteRook);
    while (w_rooks) {
        Square sq = pop_lsb(w_rooks);
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
    }

    Bitboard w_queens = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens) {
        Square sq = pop_lsb(w_queens);
        score += count_bits(get_queen_attacks(sq, both_occ) & ~white_occ) * 1;
    }

    // --- Black Mobility & Rook Activity ---
    Bitboard b_knights = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights) {
        Square sq = pop_lsb(b_knights);
        score -= count_bits(get_knight_attacks(sq) & ~black_occ) * 4;
    }

    Bitboard b_bishops = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops) {
        Square sq = pop_lsb(b_bishops);
        score -= count_bits(get_bishop_attacks(sq, both_occ) & ~black_occ) * 3;
    }

    Bitboard b_rooks = board.get_piece_bitboard(Piece::BlackRook);
    while (b_rooks) {
        Square sq = pop_lsb(b_rooks);
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
    }

    Bitboard b_queens = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens) {
        Square sq = pop_lsb(b_queens);
        score -= count_bits(get_queen_attacks(sq, both_occ) & ~black_occ) * 1;
    }

    return score;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;

    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);

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

        // Passed Pawn
        Bitboard passed_mask = get_passed_pawn_mask(sq, Color::White);
        if ((passed_mask & b_pawns) == EMPTY_BOARD) {
            score += 15 + 10 * rank; // More valuable as it advances
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
            score += 20;
        }

        // Passed Pawn
        Bitboard passed_mask = get_passed_pawn_mask(sq, Color::Black);
        if ((passed_mask & w_pawns) == EMPTY_BOARD) {
            score -= 15 + 10 * (7 - rank);
        }
    }

    return score;
}

int evaluateKingSafety(const Board& board, int eg_weight) {
    int score = 0;

    Bitboard w_king = board.get_piece_bitboard(Piece::WhiteKing);
    if (w_king) {
        Square ksq = get_lsb(w_king);
        if (ksq == Square::G1 || ksq == Square::H1) {
            if (board.get_piece(Square::F2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::G2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::H2) != Piece::WhitePawn) score -= 20;
        } else if (ksq == Square::C1 || ksq == Square::B1) {
            if (board.get_piece(Square::A2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::B2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::C2) != Piece::WhitePawn) score -= 20;
        }
    }

    Bitboard b_king = board.get_piece_bitboard(Piece::BlackKing);
    if (b_king) {
        Square ksq = get_lsb(b_king);
        if (ksq == Square::G8 || ksq == Square::H8) {
            if (board.get_piece(Square::F7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::G7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::H7) != Piece::BlackPawn) score += 20;
        } else if (ksq == Square::C8 || ksq == Square::B8) {
            if (board.get_piece(Square::A7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::B7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::C7) != Piece::BlackPawn) score += 20;
        }
    }

    // Scale king safety score down as we enter endgame
    return score * (256 - eg_weight) / 256;
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
              + evaluatePawnStructure(board)
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
