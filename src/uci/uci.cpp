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
#include "utils/rng.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

namespace ChessEngine {

int g_search_overhead_ms = 20;
bool g_own_book = true;
std::string g_book_file = "book.bin";
bool g_chess960 = false;

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
        if (m.getSourceSquare() == from) {
            if (promo == PieceType::None || m.getPromotionPieceType() == promo) {
                // Direct match
                if (m.getDestinationSquare() == to) {
                    return m;
                }
                // Castling notation matching across standard (e1g1) and Chess960 (e1h1) formats
                if (m.isCastling()) {
                    int rank = get_rank(from);
                    Color us = (rank == 0) ? Color::White : Color::Black;
                    Square r_sq = board.get_castling_rook(us, m.is_castle_k());
                    Square k_target = m.is_castle_k() ? make_square(6, rank) : make_square(2, rank);
                    if (to == r_sq || to == k_target) {
                        return m;
                    }
                }
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
    } else if (option_name == "Seed" || option_name == "seed") {
        try {
            uint64_t seed = std::stoull(option_value);
            set_global_seed(seed);
        } catch (...) {}
    } else if (option_name == "MultiPV" || option_name == "multipv" || option_name == "Multi PV") {
        try {
            int val = std::stoi(option_value);
            if (val >= 1 && val <= 256) {
                g_multipv = val;
            }
        } catch (...) {}
    } else if (option_name == "UCI_Chess960" || option_name == "uci_chess960" || option_name == "Chess960") {
        if (option_value == "true" || option_value == "True" || option_value == "1") {
            g_chess960 = true;
        } else if (option_value == "false" || option_value == "False" || option_value == "0") {
            g_chess960 = false;
        }
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
            std::cout << "option name UCI_Chess960 type check default false\n";
            std::cout << "option name MultiPV type spin default 1 min 1 max 256\n";
            std::cout << "option name SingularMargin type spin default 2 min 0 max 100\n";
            std::cout << "option name SingularExtension type check default true\n";
            std::cout << "option name IIR type check default true\n";
            std::cout << "option name LMP type check default true\n";
            std::cout << "option name LMPMaxDepth type spin default 8 min 1 max 16\n";
            std::cout << "option name ProbCut type check default true\n";
            std::cout << "option name ProbCutMargin type spin default 100 min 10 max 500\n";
            std::cout << "option name CorrectionHistory type check default true\n";
            std::cout << "option name DeltaMargin type spin default 200 min 0 max 1000\n";
            std::cout << "option name Seed type spin default 42 min 0 max 2147483647\n";
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
        } else if (command == "datagen") {
            parse_datagen(ss);
        } else if (command == "bench") {
            parse_bench(ss);
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
                    size_t rand_idx = rand_index(moves.size());
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

void parse_datagen(std::stringstream& ss) {
    int games = 100;
    int depth = 6;
    int random_plies = 8;
    std::string output_path = "train_data.plain";

    std::string token;
    while (ss >> token) {
        if (token == "games") {
            ss >> games;
        } else if (token == "depth") {
            ss >> depth;
        } else if (token == "output") {
            ss >> output_path;
        } else if (token == "random_plies") {
            ss >> random_plies;
        }
    }

    if (games <= 0) games = 10;
    if (depth <= 0) depth = 6;

    std::cout << "info string Starting self-play datagen: " << games << " games, depth " << depth 
              << ", opening plies " << random_plies << ", output: " << output_path << std::endl;

    std::ofstream out(output_path, std::ios::app);
    if (!out) {
        std::cout << "info string Error: could not open output file " << output_path << std::endl;
        return;
    }

    bool prev_book = g_own_book;
    g_own_book = false; // Disable opening book during self-play data generation

    uint64_t total_positions = 0;
    uint64_t total_filtered_out = 0;

    struct PositionRecord {
        std::string fen;
        int score;
        bool valid;
    };

    for (int g = 0; g < games; ++g) {
        Board board;
        board.reset_to_start();

        // 1. Play random legal opening moves to generate varied initial positions
        for (int p = 0; p < random_plies; ++p) {
            std::vector<Move> legal = generate_legal_moves(board);
            if (legal.empty()) break;
            Move m = legal[rand_index(legal.size())];
            UndoState undo;
            board.makeMove(m, undo);
        }

        std::vector<PositionRecord> game_history;
        double game_result = 0.5; // default draw
        bool game_over = false;

        for (int move_num = 0; move_num < 400 && !game_over; ++move_num) {
            // Draw conditions: 50-move rule, threefold repetition, insufficient material
            if (board.get_halfmove_clock() >= 100 || board.isRepetition() || board.is_insufficient_material()) {
                game_result = 0.5;
                game_over = true;
                break;
            }

            bool in_check = is_in_check(board, board.get_side_to_move());
            std::string fen = board.to_fen();

            // Run search
            g_tt.clear();
            g_stop_search.store(false);
            g_time_limit_soft_ms = -1;
            g_time_limit_hard_ms = -1;

            SearchResult res = search(board, depth);
            Move best_m = res.best_move;

            if (best_m.is_none()) {
                std::vector<Move> legal = generate_legal_moves(board);
                if (legal.empty()) {
                    if (in_check) {
                        // Current side to move is checkmated -> other side wins
                        game_result = (board.get_side_to_move() == Color::White) ? 0.0 : 1.0;
                    } else {
                        // Stalemate
                        game_result = 0.5;
                    }
                } else {
                    game_result = 0.5;
                }
                game_over = true;
                break;
            }

            // Filtering rules to exclude noisy/unstable positions:
            // 1. Exclude positions where king is in check
            // 2. Exclude positions with a capture move (noisy tactical transition)
            // 3. Exclude positions near mate bounds (|score| > 2000 or mate score)
            bool is_capture = best_m.isCapture();
            bool near_mate = (std::abs(res.score) >= MATE_SCORE - MAX_PLY) || (std::abs(res.score) > 2000);
            bool is_valid = !in_check && !is_capture && !near_mate;

            game_history.push_back({fen, res.score, is_valid});

            UndoState undo;
            board.makeMove(best_m, undo);
        }

        // Write valid game records with game result
        int game_valid_count = 0;
        for (const auto& rec : game_history) {
            if (rec.valid) {
                out << rec.fen << " | " << rec.score << " | " << game_result << "\n";
                game_valid_count++;
                total_positions++;
            } else {
                total_filtered_out++;
            }
        }
        out.flush();

        std::cout << "info string Datagen game " << g + 1 << "/" << games 
                  << " finished. Result: " << game_result 
                  << ", Positions saved: " << game_valid_count 
                  << " (Total saved: " << total_positions << ")" << std::endl;
    }

    g_own_book = prev_book;
    out.close();

    std::cout << "info string Datagen complete. Total valid positions: " << total_positions 
              << ", Filtered out: " << total_filtered_out << std::endl;
}

const std::vector<std::string> BENCHMARK_POSITIONS = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq - 0 6",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbqk1nr/ppp2ppp/4p3/3p4/1bPP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 2 4",
    "r1bqkb1r/pp3ppp/2n1pn2/2pp4/2PP4/2N1PN2/PP3PPP/R1BQKB1R w KQkq - 0 6",
    "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7",
    "r1b1kb1r/pp1p1ppp/1q2pn2/2p5/3nP3/1PNP1N2/PBP2PPP/R2QKB1R w KQkq - 1 7",
    "r2q1rk1/ppp1bppp/2n1bn2/3p4/3P4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 9",
    "r1b2rk1/1p2qppp/p1np1n2/4p3/2P5/2N1PN2/PP2BPPP/R2Q1RK1 w - - 0 11",
    "r2q1rk1/1pp2ppp/p1np1n2/4p1B1/2B1P1b1/2NP1N2/PPP2PPP/R2Q1RK1 w - - 0 8",
    "2r2rk1/1bqnbppp/pp1ppn2/8/2PNP3/1PN1BP2/P3B1PP/2RQ1R1K w - - 0 14",
    "r1b2rk1/ppqn1ppp/2p1pn2/3p4/2PP4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 9",
    "r1bqk2r/pp2bppp/2n1pn2/2pp4/2PP4/2N1PN2/PP2BPPP/R1BQK2R w KQkq - 0 7",
    "r2q1rk1/pb1nbppp/1p1ppn2/8/2PP4/2N1PN2/PP1BBPPP/R2Q1RK1 w - - 0 10",
    "r1b1k2r/ppp1qppp/2np1n2/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQkq - 0 7",
    "r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1BP2/PPP3PP/2KR1B1R w - - 1 10",
    "r2qk2r/pp1n1ppp/2pbpn2/3p4/2PP4/1PN1PN2/PB3PPP/R2QKB1R w KQkq - 1 8",
    "r1b2rk1/pp1n1ppp/1q1bpn2/3p4/2PP4/1PNBPN2/PB3PPP/R2QK2R w KQ - 1 9",
    "r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2NB1N2/PP3PPP/R1BQK2R w KQ - 0 8",
    "r1b1kb1r/pp2qppp/2n1pn2/2pp4/2PP4/2N1PN2/PP2BPPP/R1BQK2R w KQkq - 0 7",
    "r2q1rk1/pp1bbppp/2n1pn2/3p4/2PP4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 9",
    "r1bq1rk1/1p2bppp/p1np1n2/4p3/2P1P3/2N2N2/PP2BPPP/R1BQ1RK1 w - - 0 10",
    "r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1BP2/PPP3PP/R2QKB1R w KQ - 1 8",
    "2kr3r/pp1n1ppp/2p1pn2/8/1b1P4/2N1BN2/PPP2PPP/2KR3R w - - 0 12",
    "r1b2rk1/2q1bppp/p1n1pn2/1p1p4/2PN4/1PN1P3/PB2BPPP/R2Q1RK1 w - - 0 12",
    "r4rk1/ppqn1ppp/2pbpn2/3p4/2PP4/1PN1PN2/PB2BPPP/R2Q1RK1 w - - 0 10",
    "r1b1r1k1/pp3ppp/2nq1n2/3p4/3P4/1B3N2/PP1N1PPP/R2QR1K1 w - - 0 13",
    "r1b2rk1/pp1nqppp/2p1pn2/3p4/2PP4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 9",
    "r1b1r1k1/pp3ppp/2n2n2/3p4/3P4/2NB1N2/PP3PPP/R2QR1K1 w - - 0 12",
    "r2q1rk1/1pp1bppp/p1n1bn2/3p2B1/3P4/2NBPN2/PP3PPP/R2Q1RK1 w - - 0 10",
    "r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2NB1N2/PP2QPPP/R1B2RK1 w - - 0 9",
    "r1b2rk1/ppqn1ppp/2p1pn2/3p4/2PP4/1PNBPN2/P4PPP/R1BQ1RK1 w - - 0 10",
    "8/5k2/8/8/8/8/1R6/2K5 w - - 0 1",
    "8/8/5k2/8/8/5K2/1Q6/7q w - - 0 1",
    "8/5k2/8/p1p1p1p1/P1P1P1P1/8/5K2/8 w - - 0 1",
    "8/8/8/4k3/8/8/3R1K2/r7 w - - 0 1",
    "8/8/8/4k3/8/8/5K2/4BN2 w - - 0 1",
    "8/6k1/6p1/5p1p/7P/5QP1/5PK1/8 w - - 0 1",
    "8/5k2/1b6/8/8/1B6/5K2/8 w - - 0 1",
    "8/5k2/4n3/8/8/4N3/5K2/8 w - - 0 1",
    "r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4",
    "r1b1k2r/pppp1Npp/8/4p3/2Bn3q/6n1/PPPP3P/RNBQ2KR b kq - 1 10"
};

void run_benchmark(int depth, int threads, int hash_mb) {
    if (depth <= 0) depth = 13;
    if (threads <= 0) threads = 1;
    if (hash_mb <= 0) hash_mb = 16;

    int prev_threads = g_num_threads;
    bool prev_book = g_own_book;
    g_num_threads = threads;
    g_own_book = false;

    g_tt.resize(static_cast<size_t>(hash_mb));
    g_tt.clear();

    uint64_t total_nodes = 0;
    auto bench_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < BENCHMARK_POSITIONS.size(); ++i) {
        Board board;
        board.load_from_fen(BENCHMARK_POSITIONS[i]);
        g_tt.clear();
        g_stop_search.store(false);
        g_time_limit_soft_ms = -1;
        g_time_limit_hard_ms = -1;

        std::cout << "\nPosition [" << (i + 1) << "/" << BENCHMARK_POSITIONS.size() << "]: " 
                  << BENCHMARK_POSITIONS[i] << std::endl;

        SearchResult res = search(board, depth);
        total_nodes += res.nodes_searched;
    }

    auto bench_end = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(bench_end - bench_start).count();
    if (total_time_ms <= 0) total_time_ms = 1;

    uint64_t total_nps = (total_nodes * 1000ULL) / static_cast<uint64_t>(total_time_ms);

    std::cout << "\n===========================\n";
    std::cout << "Total time (ms) : " << total_time_ms << "\n";
    std::cout << "Nodes searched  : " << total_nodes << "\n";
    std::cout << "Nodes/second    : " << total_nps << "\n";
    std::cout << total_nodes << " nodes " << total_nps << " nps\n" << std::endl;

    g_num_threads = prev_threads;
    g_own_book = prev_book;
}

void parse_bench(std::stringstream& ss) {
    int depth = 13;
    int threads = 1;
    int hash_mb = 16;

    if (ss >> depth) {
        if (ss >> threads) {
            ss >> hash_mb;
        }
    }

    run_benchmark(depth, threads, hash_mb);
}

} // namespace ChessEngine
