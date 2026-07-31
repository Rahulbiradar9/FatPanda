#include "board.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include "board/movegen.hpp"

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

bool Board::make_move(Move m) {
    // 1. Back up state
    Board backup = *this;

    Square from = m.get_from();
    Square to = m.get_to();
    uint16_t flags = m.get_flags();
    Piece moving_piece = get_piece(from);
    Color us = side_to_move_;
    Color opponent = ~us;

    // Reset en-passant square for this move (might be set below for double pushes)
    en_passant_ = Square::None;

    // Increment clocks
    halfmove_clock_++;
    if (us == Color::Black) {
        fullmove_number_++;
    }

    // Reset halfmove clock if pawn moves or captures
    if (get_piece_type(moving_piece) == PieceType::Pawn || m.is_capture()) {
        halfmove_clock_ = 0;
    }

    // Handle captures
    if (m.is_capture()) {
        if (m.is_en_passant()) {
            Square cap_sq = make_square(get_file(to), get_rank(from));
            set_piece(cap_sq, Piece::None);
        } else {
            set_piece(to, Piece::None);
        }
    }

    // Handle promotion
    if (m.is_promo()) {
        Piece promo_piece = make_piece(us, m.get_promotion_piece_type());
        set_piece(from, Piece::None);
        set_piece(to, promo_piece);
    } else {
        // Normal move
        set_piece(from, Piece::None);
        set_piece(to, moving_piece);
    }

    // Handle castling rook movement
    if (flags == MoveFlag::CASTLE_K) {
        if (us == Color::White) {
            set_piece(Square::H1, Piece::None);
            set_piece(Square::F1, Piece::WhiteRook);
        } else {
            set_piece(Square::H8, Piece::None);
            set_piece(Square::F8, Piece::BlackRook);
        }
    } else if (flags == MoveFlag::CASTLE_Q) {
        if (us == Color::White) {
            set_piece(Square::A1, Piece::None);
            set_piece(Square::D1, Piece::WhiteRook);
        } else {
            set_piece(Square::A8, Piece::None);
            set_piece(Square::D8, Piece::BlackRook);
        }
    }

    // Handle pawn double push ep square setting
    if (m.is_double_push()) {
        int ep_rank = (us == Color::White) ? 2 : 5;
        en_passant_ = make_square(get_file(from), ep_rank);
    }

    // Update castling rights
    static constexpr std::array<uint8_t, 64> castling_mask = []() {
        std::array<uint8_t, 64> mask{};
        mask.fill(15);
        mask[static_cast<size_t>(Square::A1)] = 13; // ~WQ (15 - 2)
        mask[static_cast<size_t>(Square::H1)] = 14; // ~WK (15 - 1)
        mask[static_cast<size_t>(Square::E1)] = 12; // ~(WK | WQ) (15 - 3)
        mask[static_cast<size_t>(Square::A8)] = 7;  // ~BQ (15 - 8)
        mask[static_cast<size_t>(Square::H8)] = 11; // ~BK (15 - 4)
        mask[static_cast<size_t>(Square::E8)] = 3;  // ~(BK | BQ) (15 - 12)
        return mask;
    }();

    castling_rights_ &= (castling_mask[static_cast<size_t>(from)] & castling_mask[static_cast<size_t>(to)]);

    // Change side to move
    side_to_move_ = opponent;

    // Check if the move is legal
    if (is_in_check(*this, us)) {
        *this = backup;
        return false;
    }

    return true;
}

} // namespace ChessEngine
