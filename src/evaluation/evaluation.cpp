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
// High values encourage active and sensible piece placements.

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

} // namespace

int evaluate(const Board& board) {
    int score = 0;

    Bitboard white_occ = board.get_occupancy(Color::White);
    Bitboard black_occ = board.get_occupancy(Color::Black);
    Bitboard both_occ = board.get_occupancy(Color::None);

    // --- 1. MATERIAL & PIECE-SQUARE TABLES (PST) ---
    
    // Pawns
    Bitboard w_pawns = board.get_piece_bitboard(Piece::WhitePawn);
    while (w_pawns) {
        Square sq = pop_lsb(w_pawns);
        score += VAL_PAWN;
        score += pawn_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_pawns = board.get_piece_bitboard(Piece::BlackPawn);
    while (b_pawns) {
        Square sq = pop_lsb(b_pawns);
        score -= VAL_PAWN;
        score -= pawn_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // Knights
    Bitboard w_knights = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights) {
        Square sq = pop_lsb(w_knights);
        score += VAL_KNIGHT;
        score += knight_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_knights = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights) {
        Square sq = pop_lsb(b_knights);
        score -= VAL_KNIGHT;
        score -= knight_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // Bishops
    Bitboard w_bishops = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops) {
        Square sq = pop_lsb(w_bishops);
        score += VAL_BISHOP;
        score += bishop_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_bishops = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops) {
        Square sq = pop_lsb(b_bishops);
        score -= VAL_BISHOP;
        score -= bishop_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // Rooks
    Bitboard w_rooks = board.get_piece_bitboard(Piece::WhiteRook);
    while (w_rooks) {
        Square sq = pop_lsb(w_rooks);
        score += VAL_ROOK;
        score += rook_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_rooks = board.get_piece_bitboard(Piece::BlackRook);
    while (b_rooks) {
        Square sq = pop_lsb(b_rooks);
        score -= VAL_ROOK;
        score -= rook_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // Queens
    Bitboard w_queens = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens) {
        Square sq = pop_lsb(w_queens);
        score += VAL_QUEEN;
        score += queen_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_queens = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens) {
        Square sq = pop_lsb(b_queens);
        score -= VAL_QUEEN;
        score -= queen_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // Kings
    Bitboard w_king = board.get_piece_bitboard(Piece::WhiteKing);
    if (w_king) {
        Square sq = get_lsb(w_king);
        score += king_pst[static_cast<size_t>(sq)];
    }
    Bitboard b_king = board.get_piece_bitboard(Piece::BlackKing);
    if (b_king) {
        Square sq = get_lsb(b_king);
        score -= king_pst[static_cast<size_t>(get_black_square(sq))];
    }

    // --- 2. BISHOP PAIR BONUS ---
    if (count_bits(board.get_piece_bitboard(Piece::WhiteBishop)) >= 2) score += 50;
    if (count_bits(board.get_piece_bitboard(Piece::BlackBishop)) >= 2) score -= 50;

    // --- 3. PAWN STRUCTURE ---
    
    // White Pawns Structure
    Bitboard w_pawns_ref = board.get_piece_bitboard(Piece::WhitePawn);
    while (w_pawns_ref) {
        Square sq = pop_lsb(w_pawns_ref);
        int file = get_file(sq);

        // Doubled Pawn Check (another pawn on same file)
        Bitboard same_file_pawns = board.get_piece_bitboard(Piece::WhitePawn) & FILE_MASKS[file];
        if (count_bits(same_file_pawns) > 1) {
            score -= 15; // Doubled pawn penalty
        }

        // Isolated Pawn Check (no friendly pawns on adjacent files)
        Bitboard adjacent_files = (file > 0 ? FILE_MASKS[file - 1] : 0) | (file < 7 ? FILE_MASKS[file + 1] : 0);
        if ((adjacent_files & board.get_piece_bitboard(Piece::WhitePawn)) == EMPTY_BOARD) {
            score -= 20; // Isolated pawn penalty
        }
    }

    // Black Pawns Structure
    Bitboard b_pawns_ref = board.get_piece_bitboard(Piece::BlackPawn);
    while (b_pawns_ref) {
        Square sq = pop_lsb(b_pawns_ref);
        int file = get_file(sq);

        // Doubled Pawn Check
        Bitboard same_file_pawns = board.get_piece_bitboard(Piece::BlackPawn) & FILE_MASKS[file];
        if (count_bits(same_file_pawns) > 1) {
            score += 15; // Doubled pawn penalty (increases Black score, i.e., subtracts from White perspective)
        }

        // Isolated Pawn Check
        Bitboard adjacent_files = (file > 0 ? FILE_MASKS[file - 1] : 0) | (file < 7 ? FILE_MASKS[file + 1] : 0);
        if ((adjacent_files & board.get_piece_bitboard(Piece::BlackPawn)) == EMPTY_BOARD) {
            score += 20; // Isolated pawn penalty
        }
    }

    // --- 4. PIECE MOBILITY ---
    // Count attacked target squares for minor & major pieces

    // White Knights Mobility
    Bitboard w_knights_ref = board.get_piece_bitboard(Piece::WhiteKnight);
    while (w_knights_ref) {
        Square sq = pop_lsb(w_knights_ref);
        score += count_bits(get_knight_attacks(sq) & ~white_occ) * 4;
    }
    // Black Knights Mobility
    Bitboard b_knights_ref = board.get_piece_bitboard(Piece::BlackKnight);
    while (b_knights_ref) {
        Square sq = pop_lsb(b_knights_ref);
        score -= count_bits(get_knight_attacks(sq) & ~black_occ) * 4;
    }

    // White Bishops Mobility
    Bitboard w_bishops_ref = board.get_piece_bitboard(Piece::WhiteBishop);
    while (w_bishops_ref) {
        Square sq = pop_lsb(w_bishops_ref);
        score += count_bits(get_bishop_attacks(sq, both_occ) & ~white_occ) * 3;
    }
    // Black Bishops Mobility
    Bitboard b_bishops_ref = board.get_piece_bitboard(Piece::BlackBishop);
    while (b_bishops_ref) {
        Square sq = pop_lsb(b_bishops_ref);
        score -= count_bits(get_bishop_attacks(sq, both_occ) & ~black_occ) * 3;
    }

    // White Rooks Mobility
    Bitboard w_rooks_ref = board.get_piece_bitboard(Piece::WhiteRook);
    while (w_rooks_ref) {
        Square sq = pop_lsb(w_rooks_ref);
        score += count_bits(get_rook_attacks(sq, both_occ) & ~white_occ) * 2;
    }
    // Black Rooks Mobility
    Bitboard b_rooks_ref = board.get_piece_bitboard(Piece::BlackRook);
    while (b_rooks_ref) {
        Square sq = pop_lsb(b_rooks_ref);
        score -= count_bits(get_rook_attacks(sq, both_occ) & ~black_occ) * 2;
    }

    // White Queens Mobility
    Bitboard w_queens_ref = board.get_piece_bitboard(Piece::WhiteQueen);
    while (w_queens_ref) {
        Square sq = pop_lsb(w_queens_ref);
        score += count_bits(get_queen_attacks(sq, both_occ) & ~white_occ) * 1;
    }
    // Black Queens Mobility
    Bitboard b_queens_ref = board.get_piece_bitboard(Piece::BlackQueen);
    while (b_queens_ref) {
        Square sq = pop_lsb(b_queens_ref);
        score -= count_bits(get_queen_attacks(sq, both_occ) & ~black_occ) * 1;
    }

    // --- 5. KING SAFETY (PAWN SHIELD) ---
    
    // White King Shield
    if (w_king) {
        Square ksq = get_lsb(w_king);
        if (ksq == Square::G1 || ksq == Square::H1) {
            // King-side Castled: Check F2, G2, H2 pawns
            if (board.get_piece(Square::F2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::G2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::H2) != Piece::WhitePawn) score -= 20;
        } else if (ksq == Square::C1 || ksq == Square::B1) {
            // Queen-side Castled: Check A2, B2, C2 pawns
            if (board.get_piece(Square::A2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::B2) != Piece::WhitePawn) score -= 20;
            if (board.get_piece(Square::C2) != Piece::WhitePawn) score -= 20;
        }
    }

    // Black King Shield
    if (b_king) {
        Square ksq = get_lsb(b_king);
        if (ksq == Square::G8 || ksq == Square::H8) {
            // King-side Castled: Check F7, G7, H7 pawns
            if (board.get_piece(Square::F7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::G7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::H7) != Piece::BlackPawn) score += 20;
        } else if (ksq == Square::C8 || ksq == Square::B8) {
            // Queen-side Castled: Check A7, B7, C7 pawns
            if (board.get_piece(Square::A7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::B7) != Piece::BlackPawn) score += 20;
            if (board.get_piece(Square::C7) != Piece::BlackPawn) score += 20;
        }
    }

    return score;
}

} // namespace ChessEngine
