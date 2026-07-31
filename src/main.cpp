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

    return 0;
}
