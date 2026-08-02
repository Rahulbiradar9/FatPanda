#include "board.hpp"
#include <sstream>
#include <vector>
#include <cctype>

namespace ChessEngine {

bool Board::load_from_fen(std::string_view fen) {
    // Clear the board first
    clear();

    // Parse FEN parts
    std::string fen_str(fen);
    std::stringstream ss(fen_str);
    std::string placement, side, castling, en_passant, halfmove, fullmove;

    // Standard FEN must have at least 4 fields, but we support up to 6.
    if (!(ss >> placement >> side)) {
        return false;
    }

    // Default optional fields
    castling = "-";
    en_passant = "-";
    halfmove = "0";
    fullmove = "1";

    ss >> castling >> en_passant >> halfmove >> fullmove;

    // 1. Parse Piece Placement
    int rank = 7;
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8) return false; // Rank must contain exactly 8 files of square counts/pieces
            rank--;
            file = 0;
            if (rank < 0) return false; // Too many ranks in FEN
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            int empty_squares = c - '0';
            file += empty_squares;
            if (file > 8) return false; // Exceeds file bounds
        } else {
            if (file >= 8 || rank < 0) return false;

            Piece p = Piece::None;
            switch (c) {
                case 'P': p = Piece::WhitePawn; break;
                case 'N': p = Piece::WhiteKnight; break;
                case 'B': p = Piece::WhiteBishop; break;
                case 'R': p = Piece::WhiteRook; break;
                case 'Q': p = Piece::WhiteQueen; break;
                case 'K': p = Piece::WhiteKing; break;
                case 'p': p = Piece::BlackPawn; break;
                case 'n': p = Piece::BlackKnight; break;
                case 'b': p = Piece::BlackBishop; break;
                case 'r': p = Piece::BlackRook; break;
                case 'q': p = Piece::BlackQueen; break;
                case 'k': p = Piece::BlackKing; break;
                default: return false; // Unknown piece character
            }

            set_piece(make_square(file, rank), p);
            file++;
        }
    }

    // Make sure we completed exactly rank 0 and file 8
    if (rank != 0 || file != 8) return false;

    // 2. Parse Side to Move
    if (side == "w") {
        side_to_move_ = Color::White;
    } else if (side == "b") {
        side_to_move_ = Color::Black;
    } else {
        return false;
    }

    // 3. Parse Castling Rights
    castling_rights_ = Castling::NONE;
    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': castling_rights_ |= Castling::WK; break;
                case 'Q': castling_rights_ |= Castling::WQ; break;
                case 'k': castling_rights_ |= Castling::BK; break;
                case 'q': castling_rights_ |= Castling::BQ; break;
                default: return false; // Invalid castling character
            }
        }
    }

    // 4. Parse En Passant Square
    if (en_passant == "-") {
        en_passant_ = Square::None;
    } else {
        if (en_passant.length() != 2) return false;
        int ep_file = en_passant[0] - 'a';
        int ep_rank = en_passant[1] - '1';
        if (ep_file < 0 || ep_file > 7 || ep_rank < 0 || ep_rank > 7) {
            return false;
        }
        en_passant_ = make_square(ep_file, ep_rank);
    }

    // 5. Parse Clocks
    try {
        halfmove_clock_ = std::stoi(halfmove);
        fullmove_number_ = std::stoi(fullmove);
    } catch (...) {
        return false; // Non-integer clock values
    }

    if (halfmove_clock_ < 0 || fullmove_number_ <= 0) {
        return false;
    }

    hash_key_ = compute_hash_key();
    return true;
}

std::string Board::to_fen() const {
    std::stringstream fen;

    // Helper to get character representing a piece
    auto get_piece_char = [](Piece p) -> char {
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
            default:                 return '?';
        }
    };

    // 1. Piece Placement
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            Square sq = make_square(file, rank);
            Piece p = get_piece(sq);

            if (p == Piece::None) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    fen << empty_count;
                    empty_count = 0;
                }
                fen << get_piece_char(p);
            }
        }
        if (empty_count > 0) {
            fen << empty_count;
        }
        if (rank > 0) {
            fen << '/';
        }
    }

    // 2. Active Color
    fen << ' ' << (side_to_move_ == Color::White ? 'w' : 'b');

    // 3. Castling Rights
    fen << ' ';
    if (castling_rights_ == Castling::NONE) {
        fen << '-';
    } else {
        if (castling_rights_ & Castling::WK) fen << 'K';
        if (castling_rights_ & Castling::WQ) fen << 'Q';
        if (castling_rights_ & Castling::BK) fen << 'k';
        if (castling_rights_ & Castling::BQ) fen << 'q';
    }

    // 4. En Passant Square
    fen << ' ';
    if (en_passant_ == Square::None) {
        fen << '-';
    } else {
        int file = get_file(en_passant_);
        int rank = get_rank(en_passant_);
        char file_char = static_cast<char>('a' + file);
        fen << file_char << (rank + 1);
    }

    // 5. Halfmove Clock & Fullmove Number
    fen << ' ' << halfmove_clock_ << ' ' << fullmove_number_;

    return fen.str();
}

} // namespace ChessEngine
