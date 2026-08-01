#include "chess_types.hpp"

namespace ChessEngine {

std::string squareToString(Square sq) {
    if (sq == Square::None) return "-";
    int file = static_cast<int>(sq) & 7;
    int rank = static_cast<int>(sq) >> 3;
    std::string s;
    s += static_cast<char>('a' + file);
    s += static_cast<char>('1' + rank);
    return s;
}

Square stringToSquare(std::string_view str) {
    if (str == "-") return Square::None;
    if (str.length() < 2) return Square::None;
    int file = str[0] - 'a';
    int rank = str[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return Square::None;
    return static_cast<Square>((rank << 3) + file);
}

} // namespace ChessEngine
