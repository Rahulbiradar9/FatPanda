#include "evaluation.hpp"
#include "board/attacks.hpp"
#include "board/types.hpp"

namespace ChessEngine {

namespace {

// Classical Material Values
constexpr int VAL_PAWN = 100;
constexpr int VAL_KNIGHT = 320;
constexpr int VAL_BISHOP = 330;
constexpr int VAL_ROOK = 500;
constexpr int VAL_QUEEN = 900;

// Classical Piece-Square Tables (White perspective, LERF mapping)
constexpr std::array<int, 64> pawn_pst = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

constexpr std::array<int, 64> knight_pst = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr std::array<int, 64> bishop_pst = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr std::array<int, 64> rook_pst = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
       0,  0,  0,  5,  5,  0,  0,  0
};

constexpr std::array<int, 64> queen_pst = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr std::array<int, 64> king_pst = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

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
    score += count_bits(board.get_piece_bitboard(Piece::WhitePawn)) * VAL_PAWN;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackPawn)) * VAL_PAWN;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteKnight)) * VAL_KNIGHT;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackKnight)) * VAL_KNIGHT;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) * VAL_BISHOP;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackBishop)) * VAL_BISHOP;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteRook)) * VAL_ROOK;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackRook)) * VAL_ROOK;

    score += count_bits(board.get_piece_bitboard(Piece::WhiteQueen)) * VAL_QUEEN;
    score -= count_bits(board.get_piece_bitboard(Piece::BlackQueen)) * VAL_QUEEN;

    // Bishop Pair Bonus
    if (count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) >= 2) score += 50;
    if (count_bits(board.get_piece_bitboard(Piece::BlackBishop)) >= 2) score -= 50;

    return score;
}

int evaluatePieceSquareTables(const Board& board) {
    int score = 0;

    // White Pieces
    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    while (w_pawns) score += pawn_pst[static_cast<size_t>(pop_lsb(w_pawns))];

    Bitboard w_knights = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights) score += knight_pst[static_cast<size_t>(pop_lsb(w_knights))];

    Bitboard w_bishops = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops) score += bishop_pst[static_cast<size_t>(pop_lsb(w_bishops))];

    Bitboard w_rooks = board.get_piece_bitboard(Piece::WhiteRook);
    while (w_rooks) score += rook_pst[static_cast<size_t>(pop_lsb(w_rooks))];

    Bitboard w_queens = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens) score += queen_pst[static_cast<size_t>(pop_lsb(w_queens))];

    Bitboard w_king = board.get_piece_bitboard(Piece::WhiteKing);
    if (w_king) score += king_pst[static_cast<size_t>(get_lsb(w_king))];

    // Black Pieces
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);
    while (b_pawns) score -= pawn_pst[static_cast<size_t>(get_black_square(pop_lsb(b_pawns)))];

    Bitboard b_knights = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights) score -= knight_pst[static_cast<size_t>(get_black_square(pop_lsb(b_knights)))];

    Bitboard b_bishops = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops) score -= bishop_pst[static_cast<size_t>(get_black_square(pop_lsb(b_bishops)))];

    Bitboard b_rooks = board.get_piece_bitboard(Piece::BlackRook);
    while (b_rooks) score -= rook_pst[static_cast<size_t>(get_black_square(pop_lsb(b_rooks)))];

    Bitboard b_queens = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens) score -= queen_pst[static_cast<size_t>(get_black_square(pop_lsb(b_queens)))];

    Bitboard b_king = board.get_piece_bitboard(Piece::BlackKing);
    if (b_king) score -= king_pst[static_cast<size_t>(get_black_square(get_lsb(b_king)))];

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

int evaluateKingSafety(const Board& board) {
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

    return score;
}

// Combine all elements to return side-to-move perspective score
int evaluate(const Board& board) {
    int score = evaluateMaterial(board)
              + evaluatePieceSquareTables(board)
              + evaluateMobility(board)
              + evaluatePawnStructure(board)
              + evaluateKingSafety(board);

    return (board.get_side_to_move() == Color::White) ? score : -score;
}

} // namespace ChessEngine
