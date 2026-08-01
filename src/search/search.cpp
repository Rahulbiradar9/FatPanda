#include "search.hpp"
#include "evaluation/evaluation.hpp"
#include "board/movegen.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>

namespace ChessEngine {

namespace {

// Helper to determine piece values for move ordering (MVV-LVA)
int get_piece_value(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return 100;
        case PieceType::Knight: return 320;
        case PieceType::Bishop: return 330;
        case PieceType::Rook:   return 500;
        case PieceType::Queen:  return 900;
        case PieceType::King:   return 20000;
        default:                return 0;
    }
}

// Assigns a heuristic score to a move to assist in move ordering.
// PV moves, promotions, and captures are prioritized.
int score_move(const Board& board, Move move, Move pv_move) {
    if (move == pv_move) {
        return 30000; // Search the principal variation (PV) move first
    }

    if (move.is_promo()) {
        int promo_val = 0;
        switch (move.get_promotion_piece_type()) {
            case PieceType::Queen:  promo_val = 900; break;
            case PieceType::Rook:   promo_val = 500; break;
            case PieceType::Bishop: promo_val = 330; break;
            case PieceType::Knight: promo_val = 320; break;
            default: break;
        }
        return 20000 + promo_val;
    }

    if (move.is_capture()) {
        PieceType victim_type = PieceType::Pawn;
        if (move.is_en_passant()) {
            victim_type = PieceType::Pawn;
        } else {
            victim_type = get_piece_type(board.get_piece(move.get_to()));
        }

        PieceType attacker_type = get_piece_type(board.get_piece(move.get_from()));

        // MVV-LVA (Most Valuable Victim - Least Valuable Aggressor)
        return 10000 + (get_piece_value(victim_type) * 10) - (get_piece_value(attacker_type) / 100);
    }

    return 0; // Quiet moves
}

// Sort moves in place in descending order of their heuristic scores
void order_moves(const Board& board, std::vector<Move>& moves, Move pv_move) {
    std::vector<std::pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());

    for (Move m : moves) {
        scored_moves.emplace_back(score_move(board, m, pv_move), m);
    }

    std::sort(scored_moves.begin(), scored_moves.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    for (size_t i = 0; i < moves.size(); ++i) {
        moves[i] = scored_moves[i].second;
    }
}

// Generate legal captures and promotions (noisy moves) for Quiescence Search
std::vector<Move> generate_legal_captures(Board& board) {
    std::vector<Move> moves = generate_legal_moves(board);
    std::vector<Move> captures;
    captures.reserve(moves.size() / 2);
    for (Move m : moves) {
        if (m.is_capture() || m.is_promo()) {
            captures.push_back(m);
        }
    }
    return captures;
}

// Quiescence Search
int quiescence(Board& board, int alpha, int beta, int ply, SearchInfo& info) {
    info.nodes_searched++;

    // Safety guard for maximum recursion depth
    if (ply >= MAX_PLY) {
        int eval = evaluate(board);
        return board.get_side_to_move() == Color::White ? eval : -eval;
    }

    // 50-move rule check
    if (board.get_halfmove_clock() >= 100) {
        return 0;
    }

    bool in_check = is_in_check(board, board.get_side_to_move());

    // Standing pat evaluation (only allowed if not in check)
    if (!in_check) {
        int stand_pat = evaluate(board);
        if (board.get_side_to_move() == Color::Black) {
            stand_pat = -stand_pat;
        }

        if (stand_pat >= beta) {
            return stand_pat; // Beta cutoff
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    // Generate moves: if in check, generate all moves; otherwise, only captures/promotions
    std::vector<Move> moves = in_check ? generate_legal_moves(board) : generate_legal_captures(board);
    
    // Checkmate/stalemate check inside quiescence search if we are in check and have no moves
    if (in_check && moves.empty()) {
        return -MATE_SCORE + ply;
    }

    order_moves(board, moves, MOVE_NONE);

    for (Move m : moves) {
        Board next_board = board;
        if (!next_board.make_move(m)) {
            continue;
        }

        int score = -quiescence(next_board, -beta, -alpha, ply + 1, info);

        if (score >= beta) {
            return score; // Beta cutoff
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

// Recursive Negamax with Alpha-Beta Pruning
int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info) {
    info.nodes_searched++;

    // Safety guard
    if (ply >= MAX_PLY) {
        int eval = evaluate(board);
        return board.get_side_to_move() == Color::White ? eval : -eval;
    }

    // 50-move rule check
    if (board.get_halfmove_clock() >= 100) {
        return 0;
    }

    // Base case: leaf node
    if (depth <= 0) {
        return quiescence(board, alpha, beta, ply, info);
    }

    std::vector<Move> moves = generate_legal_moves(board);

    // Stalemate or Checkmate
    if (moves.empty()) {
        if (is_in_check(board, board.get_side_to_move())) {
            return -MATE_SCORE + ply; // Checkmate
        }
        return 0; // Stalemate
    }

    // Order moves
    order_moves(board, moves, MOVE_NONE);

    int best_score = -INFINITY_SCORE;
    for (Move m : moves) {
        Board next_board = board;
        if (!next_board.make_move(m)) {
            continue;
        }

        int score = -search_alphabeta(next_board, depth - 1, -beta, -alpha, ply + 1, info);

        if (score > best_score) {
            best_score = score;
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }

    return best_score;
}

// Search root at specific depth
SearchResult search_root(Board& board, int depth, SearchInfo& info) {
    SearchResult result;
    result.best_move = MOVE_NONE;
    result.score = -INFINITY_SCORE;

    std::vector<Move> moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (is_in_check(board, board.get_side_to_move())) {
            result.score = -MATE_SCORE;
        } else {
            result.score = 0;
        }
        return result;
    }

    // 50-move rule check
    if (board.get_halfmove_clock() >= 100) {
        result.score = 0;
        result.best_move = moves[0]; // fallback
        return result;
    }

    // Order moves, placing the PV move (best move from previous depth) first
    order_moves(board, moves, info.pv_move);

    int alpha = -INFINITY_SCORE;
    int beta = INFINITY_SCORE;

    for (Move m : moves) {
        Board next_board = board;
        if (!next_board.make_move(m)) {
            continue;
        }

        int score = -search_alphabeta(next_board, depth - 1, -beta, -alpha, 1, info);

        if (score > result.score) {
            result.score = score;
            result.best_move = m;
        }

        if (score > alpha) {
            alpha = score;
        }
    }

    return result;
}

} // namespace

SearchResult search(Board& board, int max_depth) {
    SearchInfo info;
    info.nodes_searched = 0;
    info.pv_move = MOVE_NONE;

    SearchResult final_result;

    for (int depth = 1; depth <= max_depth; ++depth) {
        SearchResult result = search_root(board, depth, info);

        if (result.best_move.is_none()) {
            if (depth == 1) {
                final_result = result;
            }
            break;
        }

        final_result = result;
        info.pv_move = result.best_move; // Save PV move for next iteration

        // Format and print UCI info string
        std::cout << "info depth " << depth;
        
        // Print score
        if (std::abs(result.score) > MATE_SCORE - MAX_PLY) {
            int mate_in_plies = MATE_SCORE - std::abs(result.score);
            int mate_in_moves = (mate_in_plies + 1) / 2;
            std::cout << " score mate " << (result.score > 0 ? mate_in_moves : -mate_in_moves);
        } else {
            std::cout << " score cp " << result.score;
        }

        std::cout << " nodes " << info.nodes_searched
                  << " pv " << result.best_move.to_string()
                  << std::endl;
    }

    return final_result;
}

} // namespace ChessEngine
