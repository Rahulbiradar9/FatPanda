#include "move.hpp"
#include "board/board.hpp"

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

    Square from = get_from();
    Square to = get_to();

    if (is_castle() && !g_chess960) {
        // Standard UCI notation: e1g1, e1c1, e8g8, e8c8
        int rank = get_rank(from);
        if (is_castle_k()) {
            to = make_square(6, rank); // g1 or g8
        } else if (is_castle_q()) {
            to = make_square(2, rank); // c1 or c8
        }
    }
    // In Chess960, `to` is already the rook's starting square (king-captures-rook notation)

    std::string uci = square_to_coord(from) + square_to_coord(to);

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
