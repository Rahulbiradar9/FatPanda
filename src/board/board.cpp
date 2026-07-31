#include "board.hpp"
#include <iostream>
#include <iomanip>
#include <string>

namespace ChessEngine {

namespace {

// Helper to convert piece to ASCII character
char piece_to_char(Piece p) {
    switch (p) {
        case Piece::WhitePawn:   return 'P';
        case Piece::WhiteKnight: return 'N';
        case Piece::WhiteBishop: return 'B';
        case Piece::WhiteRook:   return 'R';
        case Piece::WhiteQueen:  return 'Q';
        case Piece::WhiteKing:   return 'K';
        case Piece::BlackPawn:   return 'p';
        case Piece::BlackKnight: return 'n';
        case Piece::BlackBishop: return 'b';
        case Piece::BlackRook:   return 'r';
        case Piece::BlackQueen:  return 'q';
        case Piece::BlackKing:   return 'k';
        default:                 return '.';
    }
}

// Helper to convert file index to char ('a'-'h')
char file_to_char(int file) {
    return static_cast<char>('a' + file);
}

// Helper to convert Square to coordinate string (e.g. "e4")
std::string square_to_string(Square sq) {
    if (sq == Square::None) return "-";
    int file = get_file(sq);
    int rank = get_rank(sq);
    std::string s;
    s += file_to_char(file);
    s += std::to_string(rank + 1);
    return s;
}

} // namespace

Board::Board() {
    clear();
}

void Board::clear() {
    // Clear all piece bitboards
    pieces_.fill(EMPTY_BOARD);

    // Clear occupancy bitboards
    occupancies_.fill(EMPTY_BOARD);

    // Reset squares array to empty
    board_squares_.fill(Piece::None);

    // Reset game state
    side_to_move_ = Color::White;
    castling_rights_ = Castling::NONE;
    en_passant_ = Square::None;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
}

void Board::set_piece(Square sq, Piece p) {
    if (!is_valid_square(sq)) return;

    size_t sq_idx = static_cast<size_t>(sq);
    Piece old_piece = board_squares_[sq_idx];

    // If the piece is already the same, do nothing
    if (old_piece == p) return;

    // Remove the old piece if present
    if (old_piece != Piece::None) {
        size_t old_piece_idx = static_cast<size_t>(old_piece);
        clear_bit(pieces_[old_piece_idx], sq);

        Color old_color = get_piece_color(old_piece);
        size_t old_color_idx = static_cast<size_t>(old_color);
        clear_bit(occupancies_[old_color_idx], sq);
        clear_bit(occupancies_[2], sq); // both
    }

    // Add the new piece if not Piece::None
    if (p != Piece::None) {
        size_t new_piece_idx = static_cast<size_t>(p);
        set_bit(pieces_[new_piece_idx], sq);

        Color new_color = get_piece_color(p);
        size_t new_color_idx = static_cast<size_t>(new_color);
        set_bit(occupancies_[new_color_idx], sq);
        set_bit(occupancies_[2], sq); // both
    }

    // Update board array
    board_squares_[sq_idx] = p;
}

void Board::reset_to_start() {
    clear();

    // 1. Setup White Pieces
    // Back rank (rank 1, index 0)
    set_piece(Square::A1, Piece::WhiteRook);
    set_piece(Square::B1, Piece::WhiteKnight);
    set_piece(Square::C1, Piece::WhiteBishop);
    set_piece(Square::D1, Piece::WhiteQueen);
    set_piece(Square::E1, Piece::WhiteKing);
    set_piece(Square::F1, Piece::WhiteBishop);
    set_piece(Square::G1, Piece::WhiteKnight);
    set_piece(Square::H1, Piece::WhiteRook);
    // Pawns (rank 2, index 1)
    for (int file = 0; file < 8; ++file) {
        set_piece(make_square(file, 1), Piece::WhitePawn);
    }

    // 2. Setup Black Pieces
    // Back rank (rank 8, index 7)
    set_piece(Square::A8, Piece::BlackRook);
    set_piece(Square::B8, Piece::BlackKnight);
    set_piece(Square::C8, Piece::BlackBishop);
    set_piece(Square::D8, Piece::BlackQueen);
    set_piece(Square::E8, Piece::BlackKing);
    set_piece(Square::F8, Piece::BlackBishop);
    set_piece(Square::G8, Piece::BlackKnight);
    set_piece(Square::H8, Piece::BlackRook);
    // Pawns (rank 7, index 6)
    for (int file = 0; file < 8; ++file) {
        set_piece(make_square(file, 6), Piece::BlackPawn);
    }

    // 3. Setup Game State
    side_to_move_ = Color::White;
    castling_rights_ = Castling::ALL;
    en_passant_ = Square::None;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
}

void Board::print() const {
    std::cout << "\n  +---+---+---+---+---+---+---+---+\n";

    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            Square sq = make_square(file, rank);
            Piece p = get_piece(sq);
            std::cout << piece_to_char(p) << " | ";
        }
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    }

    std::cout << "    a   b   c   d   e   f   g   h\n\n";

    // Print other state info
    std::cout << "Side to move : " << (side_to_move_ == Color::White ? "White" : "Black") << "\n";
    
    std::cout << "Castling     : ";
    if (castling_rights_ == Castling::NONE) {
        std::cout << "-";
    } else {
        if (castling_rights_ & Castling::WK) std::cout << "K";
        if (castling_rights_ & Castling::WQ) std::cout << "Q";
        if (castling_rights_ & Castling::BK) std::cout << "k";
        if (castling_rights_ & Castling::BQ) std::cout << "q";
    }
    std::cout << "\n";

    std::cout << "En Passant   : " << square_to_string(en_passant_) << "\n";
    std::cout << "Halfmove     : " << halfmove_clock_ << "\n";
    std::cout << "Fullmove     : " << fullmove_number_ << "\n\n";
}

} // namespace ChessEngine
