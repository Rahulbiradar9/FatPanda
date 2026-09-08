#include "search.hpp"
#include "evaluation/evaluation.hpp"
#include "board/movegen.hpp"
#include "board/attacks.hpp"
#include "board/syzygy.hpp"
#include "hash/tt.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>

namespace ChessEngine {

std::atomic<bool> g_stop_search{false};
int g_num_threads = 1;
std::chrono::steady_clock::time_point g_start_time;
int g_time_limit_soft_ms = -1;
int g_time_limit_hard_ms = -1;

SearchSettings g_search_settings;
int g_singular_margin = 2;

namespace {

int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info, Move excluded_move = MOVE_NONE);

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

// Static Exchange Evaluation (SEE)
int see(const Board& board, Move move) {
    Square from = move.getSourceSquare();
    Square to = move.getDestinationSquare();
    
    if (!move.isCapture()) {
        return 0;
    }
    
    PieceType victim = get_piece_type(board.get_piece(to));
    if (move.isEnPassant()) {
        victim = PieceType::Pawn;
    }
    PieceType attacker = get_piece_type(board.get_piece(from));
    
    int attacker_val = get_piece_value(attacker);
    int victim_val = get_piece_value(victim);
    
    if (victim_val >= attacker_val) {
        return victim_val - attacker_val;
    }
    
    int gain[32];
    int d = 0;
    gain[d] = victim_val;
    
    // Copy occupancies and pieces locally
    Bitboard local_occupancy = board.get_occupancy(Color::None);
    
    std::array<Bitboard, 12> local_pieces;
    for (int p = 0; p < 12; ++p) {
        local_pieces[p] = board.get_piece_bitboard(static_cast<Piece>(p));
    }
    
    // Make the first move
    clear_bit(local_occupancy, from);
    set_bit(local_occupancy, to); // target is still occupied
    clear_bit(local_pieces[static_cast<size_t>(make_piece(board.get_side_to_move(), attacker))], from);
    
    Color us = ~board.get_side_to_move();
    int current_attacker_val = attacker_val;
    
    while (true) {
        Square next_from = Square::None;
        PieceType next_type = PieceType::None;
        
        // Find the least valuable attacker of color 'us' attacking 'to'
        // Pawn
        Bitboard pawns = local_pieces[static_cast<size_t>(make_piece(us, PieceType::Pawn))];
        Bitboard attackers = get_pawn_attacks(to, ~us) & pawns;
        if (attackers) {
            next_from = get_lsb(attackers);
            next_type = PieceType::Pawn;
        }
        // Knight
        else {
            Bitboard knights = local_pieces[static_cast<size_t>(make_piece(us, PieceType::Knight))];
            Bitboard knight_attackers = get_knight_attacks(to) & knights;
            if (knight_attackers) {
                next_from = get_lsb(knight_attackers);
                next_type = PieceType::Knight;
            }
            // Bishop
            else {
                Bitboard bishops = local_pieces[static_cast<size_t>(make_piece(us, PieceType::Bishop))];
                Bitboard bishop_attackers = get_bishop_attacks(to, local_occupancy) & bishops;
                if (bishop_attackers) {
                    next_from = get_lsb(bishop_attackers);
                    next_type = PieceType::Bishop;
                }
                // Rook
                else {
                    Bitboard rooks = local_pieces[static_cast<size_t>(make_piece(us, PieceType::Rook))];
                    Bitboard rook_attackers = get_rook_attacks(to, local_occupancy) & rooks;
                    if (rook_attackers) {
                        next_from = get_lsb(rook_attackers);
                        next_type = PieceType::Rook;
                    }
                    // Queen
                    else {
                        Bitboard queens = local_pieces[static_cast<size_t>(make_piece(us, PieceType::Queen))];
                        Bitboard queen_attackers = get_queen_attacks(to, local_occupancy) & queens;
                        if (queen_attackers) {
                            next_from = get_lsb(queen_attackers);
                            next_type = PieceType::Queen;
                        }
                        // King
                        else {
                            Bitboard king = local_pieces[static_cast<size_t>(make_piece(us, PieceType::King))];
                            Bitboard king_attackers = get_king_attacks(to) & king;
                            if (king_attackers) {
                                next_from = get_lsb(king_attackers);
                                next_type = PieceType::King;
                            }
                        }
                    }
                }
            }
        }
        
        if (next_from == Square::None) {
            break;
        }
        
        // Make the recapture
        clear_bit(local_occupancy, next_from);
        clear_bit(local_pieces[static_cast<size_t>(make_piece(us, next_type))], next_from);
        
        d++;
        gain[d] = current_attacker_val;
        current_attacker_val = get_piece_value(next_type);
        
        // If it's a King recapture, and the square is still attacked by opponent, it's illegal in actual play,
        // but for SEE we can just stop here as the king cannot be captured.
        if (next_type == PieceType::King) {
            break;
        }
        
        us = ~us;
    }
    
    // Minimax backpropagation
    while (d > 0) {
        gain[d - 1] = gain[d - 1] - std::max(0, gain[d]);
        d--;
    }
    
    return gain[0];
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
        if (g_search_settings.see) {
            int see_val = see(board, move);
            if (see_val < 0) {
                return see_val - 10000; // Penalize bad captures, placing them after quiet moves
            }
        }

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
    std::vector<Move> pseudo = generate_pseudo_legal_moves(board);
    std::vector<Move> captures;
    captures.reserve(pseudo.size() / 2);
    for (Move m : pseudo) {
        if (m.is_capture() || m.is_promo()) {
            UndoState undo;
            if (board.makeMove(m, undo)) {
                captures.push_back(m);
                board.unmakeMove(m, undo);
            }
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
        if (g_time_limit_hard_ms != -1) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - g_start_time)
                               .count();
            if (elapsed >= g_time_limit_hard_ms) {
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
    info.tt_lookups++;
    if (g_tt.probe(board.get_hash_key(), ply, tt_entry)) {
        info.tt_hits++;
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
        if (g_search_settings.see && see(board, m) < 0) {
            continue; // Prune bad captures
        }

        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        int score = -quiescence(board, -beta, -alpha, ply + 1, info);
        board.unmakeMove(m, undo);

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
int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info, Move excluded_move) {
    info.nodes_searched++;

    // Check time/stop constraints every 2048 nodes
    if ((info.nodes_searched & 2047) == 0) {
        if (g_stop_search.load()) {
            return 0;
        }
        if (g_time_limit_hard_ms != -1) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - g_start_time)
                               .count();
            if (elapsed >= g_time_limit_hard_ms) {
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

    // 50-move rule, repetition, or insufficient material check
    if (board.get_halfmove_clock() >= 100 || 
        (ply > 0 && board.isRepetition()) || 
        board.is_insufficient_material()) {
        return 0;
    }

    // Optional Syzygy WDL probing (skip in singular search)
    if (excluded_move == MOVE_NONE && g_syzygy_enabled && syzygy_is_loaded()) {
        int pieces_count = 0;
        for (int p = 0; p < 12; ++p) {
            pieces_count += count_bits(board.get_piece_bitboard(static_cast<Piece>(p)));
        }
        if (pieces_count <= 5) {
            int tb_score = 0;
            if (syzygy_probe_wdl(board, tb_score)) {
                return tb_score;
            }
        }
    }

    int original_alpha = alpha;

    // TT probe
    TTEntry tt_entry;
    info.tt_lookups++;
    bool tt_hit = g_tt.probe(board.get_hash_key(), ply, tt_entry);
    if (tt_hit && excluded_move == MOVE_NONE) {
        info.tt_hits++;
        if (tt_entry.depth >= depth) {
            if (tt_entry.flags == TT_EXACT) {
                return tt_entry.score;
            } else if (tt_entry.flags == TT_ALPHA && tt_entry.score <= alpha) {
                return alpha;
            } else if (tt_entry.flags == TT_BETA && tt_entry.score >= beta) {
                return beta;
            }
        }
    }

    bool in_check = is_in_check(board, board.get_side_to_move());

    // Reverse Futility Pruning (RFP)
    if (excluded_move == MOVE_NONE && g_search_settings.rfp && depth <= 3 && !in_check && ply > 0) {
        int margin = depth * 120;
        if (evaluate(board) - margin >= beta) {
            return beta; // Fail high
        }
    }

    // Null Move Pruning (NMP)
    if (excluded_move == MOVE_NONE && g_search_settings.nmp && depth >= 3 && !in_check && ply > 0) {
        Color us = board.get_side_to_move();
        Bitboard our_non_pawns = board.get_occupancy(us) 
                                ^ board.get_piece_bitboard(make_piece(us, PieceType::Pawn))
                                ^ board.get_piece_bitboard(make_piece(us, PieceType::King));
        if (our_non_pawns != EMPTY_BOARD) {
            UndoState undo;
            board.makeNullMove(undo);
            int R = 2; // Reduction depth
            int score = -search_alphabeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, info);
            board.unmakeNullMove(undo);
            if (score >= beta) {
                return beta; // Fail high
            }
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

    // Singular Extension:
    // Before searching the TT move, run a reduced-depth null-window search excluding the TT move
    // to test if other moves fail low against (ttScore - singularMargin).
    int extension = 0;
    if (g_search_settings.singular
        && excluded_move == MOVE_NONE
        && depth >= 8
        && tt_hit
        && tt_move != MOVE_NONE
        && (tt_entry.flags == TT_EXACT || tt_entry.flags == TT_BETA)
        && tt_entry.depth >= depth - 3
        && std::abs(tt_entry.score) < MATE_SCORE - MAX_PLY)
    {
        int singular_margin = g_singular_margin * depth;
        int singular_beta = tt_entry.score - singular_margin;
        int singular_depth = (depth - 1) / 2;

        int singular_score = search_alphabeta(board, singular_depth, singular_beta - 1, singular_beta, ply, info, tt_move);

        if (singular_score < singular_beta) {
            extension = 1;
        }
    }

    int best_score = -INFINITY_SCORE;
    Move best_move = MOVE_NONE;

    bool futility_pruning = false;
    if (g_search_settings.futility && depth == 1 && !in_check && (evaluate(board) + 150 < alpha)) {
        futility_pruning = true;
    }

    int moves_searched = 0;
    for (Move m : moves) {
        if (m == excluded_move) {
            continue;
        }

        if (futility_pruning && !m.isCapture() && !m.isPromotion() && excluded_move == MOVE_NONE) {
            continue; // Prune quiet move
        }

        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        moves_searched++;
        int ext = (m == tt_move) ? extension : 0;
        int new_depth = depth - 1 + ext;
        int score;

        // Principal Variation Search (PVS) & Late Move Reductions (LMR)
        if (g_search_settings.pvs && moves_searched > 1) {
            if (g_search_settings.lmr && depth >= 3 && moves_searched > 4 && !m.isCapture() && !m.isPromotion() && !in_check && !is_in_check(board, board.get_side_to_move())) {
                int reduction = 1;
                if (moves_searched > 12) {
                    reduction = 2;
                }
                score = -search_alphabeta(board, new_depth - reduction, -alpha - 1, -alpha, ply + 1, info);
            } else {
                score = -search_alphabeta(board, new_depth, -alpha - 1, -alpha, ply + 1, info);
            }

            if (score > alpha && score < beta) {
                // Re-search with full window
                score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info);
            }
        } else {
            // Normal alpha-beta search
            if (g_search_settings.lmr && !g_search_settings.pvs && depth >= 3 && moves_searched > 4 && !m.isCapture() && !m.isPromotion() && !in_check && !is_in_check(board, board.get_side_to_move())) {
                int reduction = 1;
                if (moves_searched > 12) {
                    reduction = 2;
                }
                score = -search_alphabeta(board, new_depth - reduction, -alpha - 1, -alpha, ply + 1, info);
                if (score > alpha) {
                    score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info);
                }
            } else {
                score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info);
            }
        }

        board.unmakeMove(m, undo);

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            alpha = score;
            
            // Update Triangular PV Table (only for regular search)
            if (excluded_move == MOVE_NONE) {
                info.pv_table[ply][ply] = m;
                for (int j = ply + 1; j < info.pv_length[ply + 1]; ++j) {
                    info.pv_table[ply][j] = info.pv_table[ply + 1][j];
                }
                info.pv_length[ply] = info.pv_length[ply + 1];
            }
        }

        if (alpha >= beta) {
            // Cutoff: if it's a quiet move, record killer and history heuristic
            if (excluded_move == MOVE_NONE && !m.isCapture() && !m.isPromotion() && ply < MAX_PLY) {
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

    if (moves_searched == 0) {
        if (excluded_move != MOVE_NONE) {
            return alpha;
        }
    }

    // TT record (only in regular search, do not corrupt TT with excluded move search)
    if (excluded_move == MOVE_NONE) {
        uint8_t flag = TT_ALPHA;
        if (best_score >= beta) {
            flag = TT_BETA;
        } else if (best_score > original_alpha) {
            flag = TT_EXACT;
        }
        g_tt.record(board.get_hash_key(), best_move, best_score, depth, flag, ply);
    }

    return best_score;
}

} // namespace

// Search root at specific depth
SearchResult search_root(Board& board, int depth, SearchInfo& info, int alpha, int beta) {
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
    info.tt_lookups++;
    if (g_tt.probe(board.get_hash_key(), 0, tt_entry) && tt_entry.move != MOVE_NONE) {
        info.tt_hits++;
        tt_move = tt_entry.move;
    }
    order_moves(board, moves, tt_move, 0, info);

    info.pv_length[0] = 0;

    for (Move m : moves) {
        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        int score = -search_alphabeta(board, depth - 1, -beta, -alpha, 1, info);
        board.unmakeMove(m, undo);

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

SearchResult search_thread(Board& board, int max_depth, int thread_id) {
    SearchInfo info;
    info.nodes_searched = 0;
    info.pv_move = MOVE_NONE;

    SearchResult final_result;
    int last_score = 0;

    std::vector<Move> root_moves = generate_legal_moves(board);
    if (root_moves.size() <= 1) {
        max_depth = 1;
    }

    int actual_max_depth = max_depth;
    if (thread_id > 0) {
        // Vary helper depth slightly
        actual_max_depth = std::max(1, max_depth + (thread_id % 3) - 1);
    }

    // Optional Syzygy root probing (only main thread probes)
    if (thread_id == 0 && g_syzygy_enabled && syzygy_is_loaded()) {
        int pieces_count = 0;
        for (int p = 0; p < 12; ++p) {
            pieces_count += count_bits(board.get_piece_bitboard(static_cast<Piece>(p)));
        }
        if (pieces_count <= 5) {
            Move tb_move;
            if (syzygy_probe_root(board, tb_move)) {
                final_result.best_move = tb_move;
                final_result.score = 0;
                final_result.nodes_searched = 1;
                final_result.search_depth = 1;
                std::cout << "bestmove " << tb_move.to_string() << std::endl;
                return final_result;
            }
        }
    }

    for (int depth = 1; depth <= actual_max_depth; ++depth) {
        // Clear PV table for this iteration
        std::fill(&info.pv_table[0][0], &info.pv_table[0][0] + MAX_PLY * MAX_PLY, MOVE_NONE);
        std::fill(&info.pv_length[0], &info.pv_length[0] + MAX_PLY, 0);

        SearchResult result;
        if (g_search_settings.aspiration && depth >= 5) {
            int alpha = last_score - 50;
            int beta = last_score + 50;
            int window = 50;
            
            while (true) {
                result = search_root(board, depth, info, alpha, beta);
                if (g_stop_search.load()) {
                    break;
                }
                
                if (result.score <= alpha) {
                    alpha = std::max(-INFINITY_SCORE, alpha - window);
                    window *= 2;
                } else if (result.score >= beta) {
                    beta = std::min(INFINITY_SCORE, beta + window);
                    window *= 2;
                } else {
                    break;
                }
            }
        } else {
            result = search_root(board, depth, info, -INFINITY_SCORE, INFINITY_SCORE);
        }

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
        last_score = result.score;

        // Set search statistics in final result
        final_result.nodes_searched = info.nodes_searched;
        final_result.search_depth = depth;
        final_result.tt_lookups = info.tt_lookups;
        final_result.tt_hits = info.tt_hits;
        if (depth > 0) {
            final_result.branching_factor = std::pow(static_cast<double>(info.nodes_searched), 1.0 / depth);
        } else {
            final_result.branching_factor = 0.0;
        }
        final_result.pv.clear();
        for (int i = 0; i < info.pv_length[0]; ++i) {
            final_result.pv.push_back(info.pv_table[0][i]);
        }

        // Format and print UCI info string (only main thread prints)
        if (thread_id == 0) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - g_start_time)
                                   .count();
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
                      << " time " << elapsed_ms
                      << " nps " << (elapsed_ms > 0 ? (info.nodes_searched * 1000 / elapsed_ms) : 0)
                      << " hash_lookups " << info.tt_lookups
                      << " hash_hits " << info.tt_hits
                      << " bf " << final_result.branching_factor
                      << " pv";
            for (Move m : final_result.pv) {
                std::cout << " " << m.to_string();
            }
            std::cout << std::endl;

            // Early stop: checkmate found
            if (std::abs(result.score) > MATE_SCORE - MAX_PLY) {
                break;
            }

            // Soft time limit check: do not start next depth if soft limit exceeded
            if (g_time_limit_soft_ms != -1) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - g_start_time)
                                   .count();
                if (elapsed >= g_time_limit_soft_ms) {
                    break;
                }
            }
        }
    }

    return final_result;
}

SearchResult search(Board& board, int max_depth) {
    if (g_num_threads <= 1) {
        return search_thread(board, max_depth, 0);
    }

    std::vector<std::thread> helpers;
    helpers.reserve(g_num_threads - 1);

    for (int i = 1; i < g_num_threads; ++i) {
        helpers.emplace_back([board, max_depth, i]() mutable {
            Board temp_board = board;
            search_thread(temp_board, max_depth, i);
        });
    }

    SearchResult result = search_thread(board, max_depth, 0);

    g_stop_search.store(true);

    for (auto& h : helpers) {
        if (h.joinable()) {
            h.join();
        }
    }

    return result;
}

} // namespace ChessEngine
