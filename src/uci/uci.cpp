#include "uci.hpp"
#include "board/board.hpp"
#include "board/movegen.hpp"
#include "search/search.hpp"
#include "hash/tt.hpp"
#include "utils/version.hpp"
#include "board/polyglot.hpp"
#include "board/syzygy.hpp"
#include "evaluation/params.hpp"
#include "evaluation/nnue.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

namespace ChessEngine {

int g_search_overhead_ms = 20;
bool g_own_book = true;
std::string g_book_file = "book.bin";

namespace {

std::thread g_search_thread;

void start_search(Board board, int depth, int soft_limit_ms, int hard_limit_ms) {
    if (g_search_thread.joinable()) {
        g_stop_search.store(true);
        g_search_thread.join();
    }
    
    g_stop_search.store(false);
    g_start_time = std::chrono::steady_clock::now();
    g_time_limit_soft_ms = soft_limit_ms;
    g_time_limit_hard_ms = hard_limit_ms;
    
    g_search_thread = std::thread([board, depth]() mutable {
        SearchResult result = search(board, depth);
        std::cout << "bestmove " << result.best_move.to_string() << std::endl;
    });
}

} // namespace

void join_search_thread() {
    g_stop_search.store(true);
    if (g_search_thread.joinable()) {
        g_search_thread.join();
    }
}

Move parse_move(Board& board, const std::string& move_str) {
    if (move_str.length() < 4) return MOVE_NONE;
    
    std::string from_str = move_str.substr(0, 2);
    std::string to_str = move_str.substr(2, 2);
    
    Square from = stringToSquare(from_str);
    Square to = stringToSquare(to_str);
    
    PieceType promo = PieceType::None;
    if (move_str.length() == 5) {
        char p = move_str[4];
        switch (p) {
            case 'q': promo = PieceType::Queen; break;
            case 'r': promo = PieceType::Rook; break;
            case 'b': promo = PieceType::Bishop; break;
            case 'n': promo = PieceType::Knight; break;
            default: break;
        }
    }
    
    auto legal_moves = generate_legal_moves(board);
    for (Move m : legal_moves) {
        if (m.getSourceSquare() == from && m.getDestinationSquare() == to) {
            if (promo == PieceType::None || m.getPromotionPieceType() == promo) {
                return m;
            }
        }
    }
    return MOVE_NONE;
}

void parse_position(Board& board, std::stringstream& ss) {
    std::string type;
    ss >> type;
    
    if (type == "startpos") {
        board.reset_to_start();
        // Skip moves token if any
        std::string word;
        if (ss >> word) {
            if (word != "moves") {
                // If the next word is not moves, try to parse it anyway
                Move m = parse_move(board, word);
                if (m != MOVE_NONE) {
                    board.make_move(m);
                }
            }
        }
    } else if (type == "fen") {
        std::string fen;
        std::string word;
        int fields = 0;
        while (ss >> word) {
            if (word == "moves") {
                break;
            }
            if (fields > 0) fen += " ";
            fen += word;
            fields++;
        }
        board.load_from_fen(fen);
    } else {
        return;
    }
    
    // Parse moves if they are present
    std::string move_str;
    while (ss >> move_str) {
        Move m = parse_move(board, move_str);
        if (m != MOVE_NONE) {
            board.make_move(m);
        }
    }
}

void parse_go(Board& board, std::stringstream& ss) {
    if (g_own_book) {
        Move book_move = lookup_book_move(board, g_book_file);
        if (book_move != MOVE_NONE) {
            std::cout << "bestmove " << book_move.to_string() << std::endl;
            return;
        }
    }

    int depth = 64; // Default max depth
    int soft_limit = -1;
    int hard_limit = -1;
    
    std::string arg;
    int wtime = -1, btime = -1, winc = 0, binc = 0, movetime = -1, movestogo = -1;
    bool infinite = false;
    
    while (ss >> arg) {
        if (arg == "depth") {
            ss >> depth;
        } else if (arg == "infinite") {
            infinite = true;
        } else if (arg == "movetime") {
            ss >> movetime;
        } else if (arg == "wtime") {
            ss >> wtime;
        } else if (arg == "btime") {
            ss >> btime;
        } else if (arg == "winc") {
            ss >> winc;
        } else if (arg == "binc") {
            ss >> binc;
        } else if (arg == "movestogo") {
            ss >> movestogo;
        }
    }
    
    if (infinite) {
        depth = 64;
        soft_limit = -1;
        hard_limit = -1;
    } else if (movetime != -1) {
        soft_limit = std::max(10, movetime - g_search_overhead_ms);
        hard_limit = std::max(10, movetime - g_search_overhead_ms);
    } else {
        int our_time = (board.get_side_to_move() == Color::White) ? wtime : btime;
        int our_inc = (board.get_side_to_move() == Color::White) ? winc : binc;
        
        if (our_time != -1) {
            int moves_left = (movestogo > 0) ? movestogo : 40;
            
            // Soft limit: target time allocation
            soft_limit = (our_time / (moves_left + 2)) + static_cast<int>(our_inc * 0.8) - g_search_overhead_ms;
            
            // Hard limit: absolute maximum time before losing on time
            hard_limit = std::min(our_time - g_search_overhead_ms, (our_time / 2) + our_inc - g_search_overhead_ms);
            
            if (soft_limit < 10) soft_limit = 10;
            if (hard_limit < 10) hard_limit = 10;
            if (soft_limit > hard_limit) soft_limit = hard_limit;
        }
    }
    
    start_search(board, depth, soft_limit, hard_limit);
}

void parse_setoption(std::stringstream& ss) {
    std::string word, option_name, option_value;
    ss >> word; // should be "name"
    
    // Read option name
    while (ss >> word && word != "value") {
        if (!option_name.empty()) option_name += " ";
        option_name += word;
    }
    
    if (word == "value") {
        std::string first_val;
        if (ss >> first_val) {
            option_value = first_val;
            std::string rest_val;
            while (ss >> rest_val) {
                option_value += " " + rest_val;
            }
        }
    }
    
    if (option_name == "Hash" || option_name == "hash") {
        try {
            int size_mb = std::stoi(option_value);
            if (size_mb > 0) {
                g_tt.resize(static_cast<size_t>(size_mb));
            }
        } catch (...) {}
    } else if (option_name == "SearchOverhead" || option_name == "searchoverhead" || option_name == "Search Overhead") {
        try {
            int overhead = std::stoi(option_value);
            if (overhead >= 0) {
                g_search_overhead_ms = overhead;
            }
        } catch (...) {}
    } else if (option_name == "OwnBook" || option_name == "ownbook" || option_name == "Own Book") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_own_book = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_own_book = false;
        }
    } else if (option_name == "BookFile" || option_name == "bookfile" || option_name == "Book File") {
        if (!option_value.empty()) {
            g_book_file = option_value;
        }
    } else if (option_name == "SyzygyPath" || option_name == "syzygypath" || option_name == "Syzygy Path") {
        if (!option_value.empty()) {
            syzygy_init(option_value);
        }
    } else if (option_name == "SyzygyUse" || option_name == "syzygyuse" || option_name == "Syzygy Use") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_syzygy_enabled = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_syzygy_enabled = false;
        }
    } else if (option_name == "Threads" || option_name == "threads") {
        try {
            int threads = std::stoi(option_value);
            if (threads >= 1 && threads <= 128) {
                g_num_threads = threads;
            }
        } catch (...) {}
    } else if (option_name == "Use NNUE" || option_name == "UseNNUE" || option_name == "use nnue" || option_name == "usennue") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_use_nnue = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_use_nnue = false;
        }
    } else if (option_name == "EvalFile" || option_name == "evalfile" || option_name == "Eval File") {
        if (!option_value.empty()) {
            g_nnue_file = option_value;
            nnue_load_file(g_nnue_file);
        }
    } else if (option_name == "SingularMargin" || option_name == "singularmargin" || option_name == "Singular Margin" || option_name == "singular_margin") {
        try {
            int margin = std::stoi(option_value);
            if (margin >= 0) {
                g_singular_margin = margin;
            }
        } catch (...) {}
    } else if (option_name == "SingularExtension" || option_name == "singularextension" || option_name == "Singular Extension") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_search_settings.singular = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_search_settings.singular = false;
        }
    } else if (option_name == "IIR" || option_name == "iir" || option_name == "InternalIterativeReduction") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_search_settings.iir = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_search_settings.iir = false;
        }
    } else if (option_name == "LMP" || option_name == "lmp" || option_name == "LateMovePruning") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_search_settings.lmp = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_search_settings.lmp = false;
        }
    } else if (option_name == "LMPMaxDepth" || option_name == "lmpmaxdepth" || option_name == "LMP Max Depth") {
        try {
            int depth = std::stoi(option_value);
            if (depth >= 1 && depth <= 16) {
                g_lmp_max_depth = depth;
            }
        } catch (...) {}
    } else if (option_name == "ProbCut" || option_name == "probcut") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_search_settings.probcut = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_search_settings.probcut = false;
        }
    } else if (option_name == "ProbCutMargin" || option_name == "probcutmargin" || option_name == "ProbCut Margin") {
        try {
            int margin = std::stoi(option_value);
            if (margin >= 0) {
                g_probcut_margin = margin;
            }
        } catch (...) {}
    } else if (option_name == "CorrectionHistory" || option_name == "correctionhistory" || option_name == "Correction History") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_search_settings.corrhist = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_search_settings.corrhist = false;
        }
    } else if (option_name == "DeltaMargin" || option_name == "deltamargin" || option_name == "Delta Margin") {
        try {
            int margin = std::stoi(option_value);
            if (margin >= 0) {
                g_delta_margin = margin;
            }
        } catch (...) {}
    }
}

void uci_loop() {
    // Flush standard output immediately after each output operation
    std::unitbuf(std::cout);
    
    std::string line;
    Board board;
    board.reset_to_start();
    
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string command;
        ss >> command;
        
        if (command == "uci") {
            std::cout << "id name FatPanda v" << get_version_string() << "\n";
            std::cout << "id author Rahul Biradar\n";
            std::cout << "option name Hash type spin default 64 min 1 max 2048\n";
            std::cout << "option name SearchOverhead type spin default 20 min 0 max 5000\n";
            std::cout << "option name OwnBook type check default true\n";
            std::cout << "option name BookFile type string default book.bin\n";
            std::cout << "option name SyzygyPath type string default <empty>\n";
            std::cout << "option name SyzygyUse type check default false\n";
            std::cout << "option name Threads type spin default 1 min 1 max 128\n";
            std::cout << "option name Use NNUE type check default false\n";
            std::cout << "option name EvalFile type string default nn.nnue\n";
            std::cout << "option name SingularMargin type spin default 2 min 0 max 100\n";
            std::cout << "option name SingularExtension type check default true\n";
            std::cout << "option name IIR type check default true\n";
            std::cout << "option name LMP type check default true\n";
            std::cout << "option name LMPMaxDepth type spin default 8 min 1 max 16\n";
            std::cout << "option name ProbCut type check default true\n";
            std::cout << "option name ProbCutMargin type spin default 100 min 10 max 500\n";
            std::cout << "option name CorrectionHistory type check default true\n";
            std::cout << "option name DeltaMargin type spin default 200 min 0 max 1000\n";
            std::cout << "uciok" << std::endl;
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "ucinewgame") {
            g_tt.clear();
            board.reset_to_start();
        } else if (command == "position") {
            parse_position(board, ss);
        } else if (command == "go") {
            parse_go(board, ss);
        } else if (command == "stop") {
            g_stop_search.store(true);
            if (g_search_thread.joinable()) {
                g_search_thread.join();
            }
        } else if (command == "setoption") {
            parse_setoption(ss);
        } else if (command == "tune") {
            parse_tune(ss);
        } else if (command == "quit") {
            g_stop_search.store(true);
            if (g_search_thread.joinable()) {
                g_search_thread.join();
            }
            break;
        } else if (command == "print" || command == "d") {
            board.print();
        }
    }

    if (g_search_thread.joinable()) {
        g_stop_search.store(true);
        g_search_thread.join();
    }
}

void parse_tune(std::stringstream& ss) {
    std::string sub_cmd;
    if (!(ss >> sub_cmd)) {
        std::cout << "info string Error: missing tune subcommand. Supported: export, import, selfplay" << std::endl;
        return;
    }
    
    if (sub_cmd == "export") {
        std::cout << "tune_params " << export_eval_params() << std::endl;
    } else if (sub_cmd == "import") {
        std::string data;
        ss >> data;
        if (data.empty()) {
            std::cout << "info string Error: missing import data" << std::endl;
        } else {
            load_eval_params(data);
            std::cout << "info string Evaluation parameters loaded successfully" << std::endl;
        }
    } else if (sub_cmd == "selfplay") {
        int games = 10;
        ss >> games;
        if (games <= 0) games = 10;
        
        std::cout << "info string Starting native self-play match of " << games << " games..." << std::endl;
        
        // Seed random number generator
        srand(static_cast<unsigned int>(time(NULL)));
        
        int tuned_wins = 0;
        int baseline_wins = 0;
        int draws = 0;
        
        EvalParams tuned_params = g_eval_params;
        EvalParams baseline_params;
        
        for (int g = 0; g < games; ++g) {
            Board board;
            board.reset_to_start();
            
            // Perturb opening with 2 random legal moves
            for (int i = 0; i < 2; ++i) {
                std::vector<Move> moves = generate_legal_moves(board);
                if (!moves.empty()) {
                    size_t rand_idx = rand() % moves.size();
                    board.make_move(moves[rand_idx]);
                }
            }
            
            bool game_over = false;
            int move_count = 0;
            
            // White is player 1, Black is player 2
            // Even games: White is Tuned, Black is Baseline
            // Odd games: White is Baseline, Black is Tuned
            bool white_is_tuned = (g % 2 == 0);
            
            while (!game_over && move_count < 150) {
                bool is_white = (board.get_side_to_move() == Color::White);
                bool current_is_tuned = (is_white && white_is_tuned) || (!is_white && !white_is_tuned);
                
                // Set parameter context
                g_eval_params = current_is_tuned ? tuned_params : baseline_params;
                
                g_stop_search = false;
                g_time_limit_soft_ms = -1;
                g_time_limit_hard_ms = -1;
                
                // Run a quick search of depth 3
                SearchResult res = search(board, 3);
                
                if (res.best_move.is_none()) {
                    // No legal moves
                    std::vector<Move> legal = generate_legal_moves(board);
                    if (legal.empty()) {
                        if (std::abs(res.score) > MATE_SCORE - MAX_PLY) {
                            if (is_white) {
                                if (white_is_tuned) baseline_wins++;
                                else tuned_wins++;
                            } else {
                                if (white_is_tuned) tuned_wins++;
                                else baseline_wins++;
                            }
                        } else {
                            draws++;
                        }
                    } else {
                        draws++;
                    }
                    game_over = true;
                    break;
                }
                
                board.make_move(res.best_move);
                move_count++;
                
                if (board.get_halfmove_clock() >= 100 || board.isRepetition() || board.is_insufficient_material()) {
                    draws++;
                    game_over = true;
                }
            }
            
            if (!game_over) {
                draws++;
            }
            
            std::cout << "info string Game " << g + 1 << "/" << games << " completed. "
                      << "Tuned Wins: " << tuned_wins << ", Baseline Wins: " << baseline_wins
                      << ", Draws: " << draws << std::endl;
        }
        
        // Restore tuned parameters
        g_eval_params = tuned_params;
        
        std::cout << "info string Match finished. Final score: Tuned " << tuned_wins 
                  << " - Baseline " << baseline_wins << " - Draws " << draws << std::endl;
    }
}

} // namespace ChessEngine
