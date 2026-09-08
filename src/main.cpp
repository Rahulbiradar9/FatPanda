#include <iostream>
#include <version>
#include "utils/version.hpp"
#include "board/board.hpp"
#include "move/move.hpp"
#include "board/movegen.hpp"
#include "evaluation/evaluation.hpp"
#include "uci/uci.hpp"
#include "utils/rng.hpp"

int main(int argc, char* argv[]) {
    // Check for --seed argument in CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--seed" || arg == "-seed" || arg == "seed") && i + 1 < argc) {
            try {
                uint64_t seed = std::stoull(argv[i + 1]);
                ChessEngine::set_global_seed(seed);
            } catch (...) {}
            break;
        }
    }

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--version" || arg == "-v" || arg == "-version" || arg == "version") {
                std::cout << "FatPanda " << ChessEngine::get_version_string() << std::endl;
                return 0;
            }
            if (arg == "--bench" || arg == "-bench" || arg == "bench") {
                int depth = 13;
                int threads = 1;
                int hash_mb = 16;
                if (i + 1 < argc && argv[i + 1][0] != '-') depth = std::stoi(argv[++i]);
                if (i + 1 < argc && argv[i + 1][0] != '-') threads = std::stoi(argv[++i]);
                if (i + 1 < argc && argv[i + 1][0] != '-') hash_mb = std::stoi(argv[++i]);
                ChessEngine::run_benchmark(depth, threads, hash_mb);
                return 0;
            }
        }
    }

    bool run_demo = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--demo" || arg == "demo") {
            run_demo = true;
            break;
        }
    }

    if (run_demo) {
        std::cout << "FatPanda v" << ChessEngine::get_version_string() << " Initialized." << std::endl;
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

        std::cout << "\nMove Representation verification:\n";
        ChessEngine::Move normal_move(ChessEngine::Square::E2, ChessEngine::Square::E4, ChessEngine::MoveFlag::DOUBLE_PUSH);
        ChessEngine::Move promo_move(ChessEngine::Square::D7, ChessEngine::Square::D8, ChessEngine::MoveFlag::PROMO_Q);
        ChessEngine::Move cap_promo(ChessEngine::Square::C7, ChessEngine::Square::D8, ChessEngine::MoveFlag::PROMO_R_CAP);
        
        std::cout << "Normal move e2-e4 (Double Push): " << normal_move.to_string() << "\n";
        std::cout << "Promotion d7-d8 to Queen       : " << promo_move.to_string() << "\n";
        std::cout << "Promotion capture c7-d8 to Rook: " << cap_promo.to_string() << "\n";

        std::cout << "\nMove Generation verification:\n";
        ChessEngine::Board test_board;
        test_board.reset_to_start();
        
        auto legal_moves = ChessEngine::generate_legal_moves(test_board);
        std::cout << "Starting position legal move count: " << legal_moves.size() << " (Expected: 20)\n";
        std::cout << "Moves: ";
        for (const auto& m : legal_moves) {
            std::cout << m.to_string() << " ";
        }
        std::cout << "\n";

        std::cout << "\nStatic Evaluation verification:\n";
        std::cout << "Starting position static evaluation score: " << ChessEngine::evaluate(test_board) << " cp\n";

        // Unbalanced custom FEN position: White has extra Queen
        ChessEngine::Board test_unbalanced;
        const std::string unbalanced_fen = "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        if (test_unbalanced.load_from_fen(unbalanced_fen)) {
            std::cout << "Extra White Queen position evaluation score: " << ChessEngine::evaluate(test_unbalanced) << " cp\n";
        }

        return 0;
    }

    // Default: enter standard UCI protocol loop
    ChessEngine::uci_loop();
    return 0;
}
