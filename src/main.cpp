#include <iostream>
#include <version>
#include "utils/version.hpp"
#include "board/board.hpp"

int main() {
    std::cout << "Chess Engine v" << ChessEngine::get_version_string() << " Initialized." << std::endl;
#if defined(__cpp_lib_three_way_comparison)
    std::cout << "C++20 standard is verified and active!" << std::endl;
#else
    std::cout << "C++20 standard detection failed. Please check compiler settings." << std::endl;
#endif

    std::cout << "\nSetting up starting position:\n";
    ChessEngine::Board board;
    board.reset_to_start();
    board.print();

    std::cout << "\nLoading a custom FEN position (En Passant on d6, side to move Black):\n";
    const std::string custom_fen = "rnbqkbnr/pp1ppppp/8/2p5/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d6 0 2";
    if (board.load_from_fen(custom_fen)) {
        board.print();
        std::cout << "Re-generated FEN: " << board.to_fen() << "\n";
    } else {
        std::cout << "Failed to parse FEN!\n";
    }

    return 0;
}
