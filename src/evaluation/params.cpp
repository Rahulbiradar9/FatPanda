#include "params.hpp"
#include <sstream>
#include <vector>
#include <iostream>

namespace ChessEngine {

EvalParams g_eval_params;

EvalParams::EvalParams() {
    pawn_pst = {
         0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
         5,  5, 10, 25, 25, 10,  5,  5,
         0,  0,  0, 20, 20,  0,  0,  0,
         5, -5,-10,  0,  0,-10, -5,  5,
         5, 10, 10,-20,-20, 10, 10,  5,
         0,  0,  0,  0,  0,  0,  0,  0
    };

    knight_pst = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };

    bishop_pst = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };

    rook_pst = {
          0,  0,  0,  0,  0,  0,  0,  0,
          5, 10, 10, 10, 10, 10, 10,  5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
           0,  0,  0,  5,  5,  0,  0,  0
    };

    queen_pst = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    king_pst = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    };

    king_endgame_pst = {
        -50,-30,-30,-30,-30,-30,-30,-50,
        -30,-10,  0,  0,  0,  0,-10,-30,
        -30,  0, 20, 30, 30, 20,  0,-30,
        -30,  0, 30, 40, 40, 30,  0,-30,
        -30,  0, 30, 40, 40, 30,  0,-30,
        -30,  0, 20, 30, 30, 20,  0,-30,
        -30,-10,  0,  0,  0,  0,-10,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };
}

static std::vector<int> parse_pst_values(const std::string& val_str) {
    std::vector<int> vals;
    std::stringstream ss(val_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            vals.push_back(std::stoi(item));
        } catch (...) {}
    }
    return vals;
}

bool load_eval_params(const std::string& data) {
    std::stringstream ss(data);
    std::string segment;
    
    while (std::getline(ss, segment, ';')) {
        if (segment.empty()) continue;
        size_t eq_pos = segment.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = segment.substr(0, eq_pos);
        std::string val_str = segment.substr(eq_pos + 1);
        
        if (key == "val_pawn") g_eval_params.val_pawn = std::stoi(val_str);
        else if (key == "val_knight") g_eval_params.val_knight = std::stoi(val_str);
        else if (key == "val_bishop") g_eval_params.val_bishop = std::stoi(val_str);
        else if (key == "val_rook") g_eval_params.val_rook = std::stoi(val_str);
        else if (key == "val_queen") g_eval_params.val_queen = std::stoi(val_str);
        else {
            std::vector<int> vals = parse_pst_values(val_str);
            if (vals.size() == 64) {
                std::array<int, 64> pst_arr;
                std::copy(vals.begin(), vals.end(), pst_arr.begin());
                
                if (key == "pawn_pst") g_eval_params.pawn_pst = pst_arr;
                else if (key == "knight_pst") g_eval_params.knight_pst = pst_arr;
                else if (key == "bishop_pst") g_eval_params.bishop_pst = pst_arr;
                else if (key == "rook_pst") g_eval_params.rook_pst = pst_arr;
                else if (key == "queen_pst") g_eval_params.queen_pst = pst_arr;
                else if (key == "king_pst") g_eval_params.king_pst = pst_arr;
                else if (key == "king_endgame_pst") g_eval_params.king_endgame_pst = pst_arr;
            }
        }
    }
    return true;
}

static std::string serialize_pst(const std::array<int, 64>& pst) {
    std::stringstream ss;
    for (size_t i = 0; i < 64; ++i) {
        ss << pst[i];
        if (i < 63) ss << ",";
    }
    return ss.str();
}

std::string export_eval_params() {
    std::stringstream ss;
    ss << "val_pawn=" << g_eval_params.val_pawn << ";";
    ss << "val_knight=" << g_eval_params.val_knight << ";";
    ss << "val_bishop=" << g_eval_params.val_bishop << ";";
    ss << "val_rook=" << g_eval_params.val_rook << ";";
    ss << "val_queen=" << g_eval_params.val_queen << ";";
    
    ss << "pawn_pst=" << serialize_pst(g_eval_params.pawn_pst) << ";";
    ss << "knight_pst=" << serialize_pst(g_eval_params.knight_pst) << ";";
    ss << "bishop_pst=" << serialize_pst(g_eval_params.bishop_pst) << ";";
    ss << "rook_pst=" << serialize_pst(g_eval_params.rook_pst) << ";";
    ss << "queen_pst=" << serialize_pst(g_eval_params.queen_pst) << ";";
    ss << "king_pst=" << serialize_pst(g_eval_params.king_pst) << ";";
    ss << "king_endgame_pst=" << serialize_pst(g_eval_params.king_endgame_pst) << ";";
    
    return ss.str();
}

} // namespace ChessEngine
