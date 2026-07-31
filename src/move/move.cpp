#include "move.hpp"

namespace ChessEngine {

std::string Move::to_string() const {
    if (is_none()) {
        return "0000"; // UCI null move representation
    }

    auto square_to_coord = [](Square sq) -> std::string {
        if (sq == Square::None) return "--";
        int file = get_file(sq);
        int rank = get_rank(sq);
        std::string s;
        s += static_cast<char>('a' + file);
        s += static_cast<char>('1' + rank);
        return s;
    };

    std::string uci = square_to_coord(get_from()) + square_to_coord(get_to());

    // Append promotion piece character if applicable
    if (is_promo()) {
        switch (get_promotion_piece_type()) {
            case PieceType::Knight: uci += 'n'; break;
            case PieceType::Bishop: uci += 'b'; break;
            case PieceType::Rook:   uci += 'r'; break;
            case PieceType::Queen:  uci += 'q'; break;
            default: break;
        }
    }

    return uci;
}

} // namespace ChessEngine
