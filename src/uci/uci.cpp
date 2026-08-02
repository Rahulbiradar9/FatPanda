#include "uci.hpp"
#include "board/board.hpp"
#include "board/movegen.hpp"
#include "search/search.hpp"
#include "hash/tt.hpp"
#include "utils/version.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

namespace ChessEngine {

namespace {

std::thread g_search_thread;

void start_search(Board board, int depth, int time_limit_ms) {
    if (g_search_thread.joinable()) {
        g_stop_search.store(true);
        g_search_thread.join();
    }
    
    g_stop_search.store(false);
    g_start_time = std::chrono::steady_clock::now();
    g_time_limit_ms = time_limit_ms;
    
    g_search_thread = std::thread([board, depth]() mutable {
        SearchResult result = search(board, depth);
        std::cout << "bestmove " << result.best_move.to_string() << std::endl;
    });
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
    int depth = 64; // Default max depth
    int time_limit_ms = -1;
    
    std::string arg;
    int wtime = -1, btime = -1, winc = 0, binc = 0, movetime = -1;
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
        }
    }
    
    if (infinite) {
        depth = 64;
        time_limit_ms = -1;
    } else if (movetime != -1) {
        time_limit_ms = movetime;
    } else {
        int our_time = (board.get_side_to_move() == Color::White) ? wtime : btime;
        int our_inc = (board.get_side_to_move() == Color::White) ? winc : binc;
        
        if (our_time != -1) {
            // Allocate 1/40th of time + half of increment
            time_limit_ms = (our_time / 40) + (our_inc / 2);
            if (time_limit_ms > our_time) {
                time_limit_ms = our_time - 50; // leave 50ms buffer
            }
            if (time_limit_ms < 10) {
                time_limit_ms = 10;
            }
        }
    }
    
    start_search(board, depth, time_limit_ms);
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
        ss >> option_value;
    }
    
    if (option_name == "Hash" || option_name == "hash") {
        try {
            int size_mb = std::stoi(option_value);
            if (size_mb > 0) {
                g_tt.resize(static_cast<size_t>(size_mb));
            }
        } catch (...) {}
    }
}

} // namespace

void uci_loop() {
    // Flush standard output immediately after each output operation
    std::cout << std::unitbuf;
    
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
            std::cout << "id name ChessEngine v" << get_version_string() << "\n";
            std::cout << "id author Rahul Biradar\n";
            std::cout << "option name Hash type spin default 64 min 1 max 2048\n";
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

} // namespace ChessEngine
