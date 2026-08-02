#include "search.hpp"
#include "evaluation/evaluation.hpp"
#include "board/movegen.hpp"
#include "hash/tt.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>

namespace ChessEngine {

std::atomic<bool> g_stop_search{false};
std::chrono::steady_clock::time_point g_start_time;
int g_time_limit_ms = -1;

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
// PV moves, promotions, captures, killers, and histories are prioritized.
int score_move(const Board& board, Move move, Move pv_move, int ply, const SearchInfo& info) {
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

    // Quiet moves: order by Killer moves, then by History heuristic
    if (ply < MAX_PLY) {
        if (move == info.killer_moves[0][ply]) {
            return 9000;
        }
        if (move == info.killer_moves[1][ply]) {
            return 8000;
        }
    }

    // Retrieve history heuristic score
    Piece p = board.get_piece(move.get_from());
    if (p != Piece::None) {
        int piece_idx = static_cast<int>(p);
        int sq_idx = static_cast<int>(move.get_to());
        int history_val = info.history_moves[piece_idx][sq_idx];
        // Scale and cap the history score to be in range [0, 7000]
        return std::min(history_val, 7000);
    }

    return 0; // Quiet moves
}

// Sort moves in place in descending order of their heuristic scores
void order_moves(const Board& board, std::vector<Move>& moves, Move pv_move, int ply, const SearchInfo& info) {
    std::vector<std::pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());

    for (Move m : moves) {
        scored_moves.emplace_back(score_move(board, m, pv_move, ply, info), m);
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

    // Check time/stop constraints every 2048 nodes
    if ((info.nodes_searched & 2047) == 0) {
        if (g_stop_search.load()) {
            return 0;
        }
        if (g_time_limit_ms != -1) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - g_start_time)
                               .count();
            if (elapsed >= g_time_limit_ms) {
                g_stop_search.store(true);
                return 0;
            }
        }
    }

    // Initialize PV length
    info.pv_length[ply] = ply;

    // Safety guard for maximum recursion depth
    if (ply >= MAX_PLY) {
        return evaluate(board);
    }

    // 50-move rule check
    if (board.get_halfmove_clock() >= 100) {
        return 0;
    }

    // TT probe in quiescence
    TTEntry tt_entry;
    if (g_tt.probe(board.get_hash_key(), ply, tt_entry)) {
        if (tt_entry.flags == TT_EXACT) return tt_entry.score;
        if (tt_entry.flags == TT_ALPHA && tt_entry.score <= alpha) return alpha;
        if (tt_entry.flags == TT_BETA && tt_entry.score >= beta) return beta;
    }

    int original_alpha = alpha;
    bool in_check = is_in_check(board, board.get_side_to_move());

    // Standing pat evaluation (only allowed if not in check)
    if (!in_check) {
        int stand_pat = evaluate(board);
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

    order_moves(board, moves, MOVE_NONE, ply, info);

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

    // TT record in quiescence
    uint8_t flag = TT_ALPHA;
    if (alpha >= beta) {
        flag = TT_BETA;
    } else if (alpha > original_alpha) {
        flag = TT_EXACT;
    }
    g_tt.record(board.get_hash_key(), MOVE_NONE, alpha, 0, flag, ply);

    return alpha;
}

// Recursive Negamax with Alpha-Beta Pruning
int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info) {
    info.nodes_searched++;

    // Check time/stop constraints every 2048 nodes
    if ((info.nodes_searched & 2047) == 0) {
        if (g_stop_search.load()) {
            return 0;
        }
        if (g_time_limit_ms != -1) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - g_start_time)
                               .count();
            if (elapsed >= g_time_limit_ms) {
                g_stop_search.store(true);
                return 0;
            }
        }
    }

    // Initialize PV length
    info.pv_length[ply] = ply;

    // Safety guard
    if (ply >= MAX_PLY) {
        return evaluate(board);
    }

    // 50-move rule check
    if (board.get_halfmove_clock() >= 100) {
        return 0;
    }

    int original_alpha = alpha;

    // TT probe
    TTEntry tt_entry;
    bool tt_hit = g_tt.probe(board.get_hash_key(), ply, tt_entry);
    if (tt_hit && tt_entry.depth >= depth) {
        if (tt_entry.flags == TT_EXACT) {
            return tt_entry.score;
        } else if (tt_entry.flags == TT_ALPHA && tt_entry.score <= alpha) {
            return alpha;
        } else if (tt_entry.flags == TT_BETA && tt_entry.score >= beta) {
            return beta;
        }
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

    // Order moves, prioritizing TT best move
    Move tt_move = tt_hit ? tt_entry.move : MOVE_NONE;
    order_moves(board, moves, tt_move, ply, info);

    int best_score = -INFINITY_SCORE;
    Move best_move = MOVE_NONE;

    for (Move m : moves) {
        Board next_board = board;
        if (!next_board.make_move(m)) {
            continue;
        }

        int score = -search_alphabeta(next_board, depth - 1, -beta, -alpha, ply + 1, info);

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            alpha = score;
            
            // Update Triangular PV Table
            info.pv_table[ply][ply] = m;
            for (int j = ply + 1; j < info.pv_length[ply + 1]; ++j) {
                info.pv_table[ply][j] = info.pv_table[ply + 1][j];
            }
            info.pv_length[ply] = info.pv_length[ply + 1];
        }

        if (alpha >= beta) {
            // Cutoff: if it's a quiet move, record killer and history heuristic
            if (!m.isCapture() && !m.isPromotion() && ply < MAX_PLY) {
                // Update killer moves
                info.killer_moves[1][ply] = info.killer_moves[0][ply];
                info.killer_moves[0][ply] = m;

                // Update history heuristic
                Piece p = board.get_piece(m.get_from());
                if (p != Piece::None) {
                    int piece_idx = static_cast<int>(p);
                    int sq_idx = static_cast<int>(m.get_to());
                    info.history_moves[piece_idx][sq_idx] += depth * depth;

                    // Prevent history overflow by aging/halving scores when any entry exceeds 100000
                    if (info.history_moves[piece_idx][sq_idx] > 100000) {
                        for (int p_idx = 0; p_idx < 12; ++p_idx) {
                            for (int sq = 0; sq < 64; ++sq) {
                                info.history_moves[p_idx][sq] /= 2;
                            }
                        }
                    }
                }
            }
            break; // Beta cutoff
        }
    }

    // TT record
    uint8_t flag = TT_ALPHA;
    if (best_score >= beta) {
        flag = TT_BETA;
    } else if (best_score > original_alpha) {
        flag = TT_EXACT;
    }
    g_tt.record(board.get_hash_key(), best_move, best_score, depth, flag, ply);

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

    // Order moves, placing the PV move (best move from previous depth) or TT move first
    TTEntry tt_entry;
    Move tt_move = info.pv_move;
    if (g_tt.probe(board.get_hash_key(), 0, tt_entry) && tt_entry.move != MOVE_NONE) {
        tt_move = tt_entry.move;
    }
    order_moves(board, moves, tt_move, 0, info);

    info.pv_length[0] = 0;

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
            
            if (score > alpha) {
                alpha = score;
                
                // Update Triangular PV Table at Root
                info.pv_table[0][0] = m;
                for (int j = 1; j < info.pv_length[1]; ++j) {
                    info.pv_table[0][j] = info.pv_table[1][j];
                }
                info.pv_length[0] = info.pv_length[1];
            }
        }
    }

    // Root node score is exact, record in TT
    if (result.best_move != MOVE_NONE) {
        g_tt.record(board.get_hash_key(), result.best_move, result.score, depth, TT_EXACT, 0);
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
        // Clear PV table for this iteration
        std::fill(&info.pv_table[0][0], &info.pv_table[0][0] + MAX_PLY * MAX_PLY, MOVE_NONE);
        std::fill(&info.pv_length[0], &info.pv_length[0] + MAX_PLY, 0);

        SearchResult result = search_root(board, depth, info);

        if (g_stop_search.load()) {
            break; // Discard partial/stopped results
        }

        if (result.best_move.is_none()) {
            if (depth == 1) {
                final_result = result;
            }
            break;
        }

        final_result = result;
        info.pv_move = result.best_move; // Save PV move for next iteration

        // Set search statistics in final result
        final_result.nodes_searched = info.nodes_searched;
        final_result.search_depth = depth;
        final_result.pv.clear();
        for (int i = 0; i < info.pv_length[0]; ++i) {
            final_result.pv.push_back(info.pv_table[0][i]);
        }

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

        std::cout << " nodes " << info.nodes_searched << " pv";
        for (Move m : final_result.pv) {
            std::cout << " " << m.to_string();
        }
        std::cout << std::endl;
    }

    return final_result;
}

} // namespace ChessEngine
