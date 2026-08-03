#pragma once
#include <array>
#include <string>

namespace ChessEngine {

struct EvalParams {
    // Material Values
    int val_pawn = 100;
    int val_knight = 320;
    int val_bishop = 330;
    int val_rook = 500;
    int val_queen = 900;

    // Piece-Square Tables (White perspective)
    std::array<int, 64> pawn_pst;
    std::array<int, 64> knight_pst;
    std::array<int, 64> bishop_pst;
    std::array<int, 64> rook_pst;
    std::array<int, 64> queen_pst;
    std::array<int, 64> king_pst;
    std::array<int, 64> king_endgame_pst;

    EvalParams();
};

extern EvalParams g_eval_params;

// Load parameter configurations from a string
bool load_eval_params(const std::string& data);

// Export current parameters to a string
std::string export_eval_params();

} // namespace ChessEngine
