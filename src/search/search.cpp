#include "search.hpp"
#include "evaluation/evaluation.hpp"
#include "board/movegen.hpp"
#include "board/attacks.hpp"
#include "board/syzygy.hpp"
#include "hash/tt.hpp"
#include "hash/zobrist.hpp"
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
int g_lmp_max_depth = 8;
int g_probcut_margin = 100;
int g_delta_margin = 200;
int g_multipv = 1;

namespace {

constexpr int HISTORY_WEIGHT_MAIN = 2;
constexpr int HISTORY_WEIGHT_CONT1 = 2;
constexpr int HISTORY_WEIGHT_CONT2 = 1;
constexpr int HISTORY_WEIGHT_TOTAL = HISTORY_WEIGHT_MAIN + HISTORY_WEIGHT_CONT1 + HISTORY_WEIGHT_CONT2;

// Internal Iterative Reduction (IIR) configuration
constexpr int IIR_MIN_DEPTH = 4;
constexpr int IIR_REDUCTION = 1;

// ProbCut configuration
constexpr int PROBCUT_MIN_DEPTH = 5;

// Precalculated Logarithmic Late Move Reduction (LMR) table
struct LmrTable {
    int table[64][64]{};
    LmrTable() {
        for (int d = 1; d < 64; ++d) {
            for (int m = 1; m < 64; ++m) {
                table[d][m] = static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25);
            }
        }
    }
} g_lmr_table;

// Late Move Pruning (LMP) move-count thresholds by depth (1..16) based on (d*d + 2*d)/2
constexpr std::array<int, 17> LMP_MOVE_THRESHOLDS = {
    0, 2, 4, 7, 12, 17, 24, 31, 40, 49, 60, 71, 84, 97, 112, 127, 144
};

// Fast computation of pawn hash key for pawn structure correction history
inline uint64_t compute_pawn_hash(const Board& board) {
    uint64_t hash = 0;
    Bitboard wp = board.get_piece_bitboard(Piece::WhitePawn);
    while (wp) {
        Square sq = pop_lsb(wp);
        hash ^= piece_keys[static_cast<size_t>(Piece::WhitePawn)][static_cast<size_t>(sq)];
    }
    Bitboard bp = board.get_piece_bitboard(Piece::BlackPawn);
    while (bp) {
        Square sq = pop_lsb(bp);
        hash ^= piece_keys[static_cast<size_t>(Piece::BlackPawn)][static_cast<size_t>(sq)];
    }
    return hash;
}

// Fast computation of non-pawn piece hash key for material/piece placement correction history
inline uint64_t compute_non_pawn_hash(const Board& board) {
    uint64_t hash = 0;
    static constexpr Piece non_pawns[] = {
        Piece::WhiteKnight, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteQueen, Piece::WhiteKing,
        Piece::BlackKnight, Piece::BlackBishop, Piece::BlackRook, Piece::BlackQueen, Piece::BlackKing
    };
    for (Piece p : non_pawns) {
        Bitboard bb = board.get_piece_bitboard(p);
        while (bb) {
            Square sq = pop_lsb(bb);
            hash ^= piece_keys[static_cast<size_t>(p)][static_cast<size_t>(sq)];
        }
    }
    return hash;
}

// Get the combined pawn + non-pawn evaluation correction (in centipawns)
inline int get_correction_value(const Board& board, const SearchInfo& info) {
    Color stm = board.get_side_to_move();
    int stm_idx = (stm == Color::White) ? 0 : 1;
    uint64_t pawn_hash = compute_pawn_hash(board);
    uint64_t non_pawn_hash = compute_non_pawn_hash(board);

    int pawn_corr = info.pawn_corr_hist[stm_idx][pawn_hash % CORRECTION_HISTORY_SIZE];
    int non_pawn_corr = info.non_pawn_corr_hist[stm_idx][non_pawn_hash % CORRECTION_HISTORY_SIZE];

    return (pawn_corr + non_pawn_corr) / CORRECTION_HISTORY_SCALE;
}

// Update a correction history entry using the gravity formula: entry += bonus - (entry * |bonus|) / MAX
inline void update_corr_entry(int& entry, int bonus) {
    entry += bonus - (entry * std::abs(bonus)) / CORRECTION_HISTORY_MAX;
}

int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info, Move excluded_move = MOVE_NONE, MoveContext prev1 = {}, MoveContext prev2 = {});

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

// Helper to compute capture / promotion value for Delta Pruning
inline int get_capture_value(const Board& board, Move move) {
    Square to = move.getDestinationSquare();
    Piece captured = board.get_piece(to);
    PieceType victim = (captured != Piece::None) ? get_piece_type(captured) : PieceType::None;
    if (move.isEnPassant()) {
        victim = PieceType::Pawn;
    }
    int val = get_piece_value(victim);
    if (move.isPromotion()) {
        PieceType promo = move.getPromotionPieceType();
        val += get_piece_value(promo) - get_piece_value(PieceType::Pawn);
    }
    return val;
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
// PV moves, promotions, captures, killers, countermoves, and histories are prioritized.
int score_move(const Board& board, Move move, Move pv_move, int ply, const SearchInfo& info, MoveContext prev1 = {}, MoveContext prev2 = {}) {
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

        Piece attacker = board.get_piece(move.get_from());
        PieceType attacker_type = get_piece_type(attacker);

        int cap_hist = 0;
        if (attacker != Piece::None && victim_type != PieceType::None) {
            cap_hist = info.capture_history[static_cast<int>(attacker)][static_cast<int>(move.get_to())][static_cast<int>(victim_type)];
        }

        // MVV-LVA (Most Valuable Victim - Least Valuable Aggressor) + Capture History
        return 10000 + (get_piece_value(victim_type) * 10) - (get_piece_value(attacker_type) / 100) + (cap_hist / 16);
    }

    // Quiet moves: order by Killer moves, Countermoves, then combined History + Continuation History
    if (ply < MAX_PLY) {
        if (move == info.killer_moves[0][ply]) {
            return 9000;
        }
        if (move == info.killer_moves[1][ply]) {
            return 8000;
        }
    }

    // Countermove heuristic
    if (prev1.piece != Piece::None && prev1.to != Square::None) {
        if (move == info.counter_moves[static_cast<int>(prev1.piece)][static_cast<int>(prev1.to)]) {
            return 7500;
        }
    }

    // Retrieve history heuristic and continuation history scores
    Piece p = board.get_piece(move.get_from());
    if (p != Piece::None) {
        int piece_idx = static_cast<int>(p);
        int sq_idx = static_cast<int>(move.get_to());
        int main_history = info.history_moves[piece_idx][sq_idx];

        int cont1_history = 0;
        if (prev1.piece != Piece::None && prev1.to != Square::None) {
            int p1_idx = static_cast<int>(prev1.piece);
            int to1_idx = static_cast<int>(prev1.to);
            cont1_history = info.cont_history_1ply[p1_idx][to1_idx][piece_idx][sq_idx];
        }

        int cont2_history = 0;
        if (prev2.piece != Piece::None && prev2.to != Square::None) {
            int p2_idx = static_cast<int>(prev2.piece);
            int to2_idx = static_cast<int>(prev2.to);
            cont2_history = info.cont_history_2ply[p2_idx][to2_idx][piece_idx][sq_idx];
        }

        int combined_history = (main_history * HISTORY_WEIGHT_MAIN 
                              + cont1_history * HISTORY_WEIGHT_CONT1 
                              + cont2_history * HISTORY_WEIGHT_CONT2) / HISTORY_WEIGHT_TOTAL;

        // Scale and cap the history score to be in range [0, 7000]
        return std::clamp(combined_history, 0, 7000);
    }

    return 0; // Quiet moves
}

// Sort moves in place in descending order of their heuristic scores
void order_moves(const Board& board, std::vector<Move>& moves, Move pv_move, int ply, const SearchInfo& info, MoveContext prev1 = {}, MoveContext prev2 = {}) {
    std::vector<std::pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());

    for (Move m : moves) {
        scored_moves.emplace_back(score_move(board, m, pv_move, ply, info, prev1, prev2), m);
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
    int stand_pat = 0;

    // Standing pat evaluation (only allowed if not in check)
    if (!in_check) {
        stand_pat = evaluate(board);
        if (g_search_settings.corrhist) {
            stand_pat += get_correction_value(board, info);
        }
        if (stand_pat >= beta) {
            return stand_pat; // Beta cutoff
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    // Generate moves: if in check, generate all moves (check evasions); otherwise, only captures/promotions
    MoveList moves;
    if (in_check) {
        generatePseudoLegalMoves(board, moves);
        for (size_t i = 0; i < moves.size(); ++i) {
            moves.scores[i] = score_move(board, moves[i], MOVE_NONE, ply, info);
        }
    } else {
        generatePseudoLegalCaptures(board, moves);
        for (size_t i = 0; i < moves.size(); ++i) {
            moves.scores[i] = score_move(board, moves[i], MOVE_NONE, ply, info);
        }
    }

    bool endgame = false;
    if (!in_check) {
        int non_pawn_count = count_bits(board.get_occupancy(Color::None) 
                                      ^ board.get_piece_bitboard(Piece::WhitePawn) 
                                      ^ board.get_piece_bitboard(Piece::BlackPawn) 
                                      ^ board.get_piece_bitboard(Piece::WhiteKing) 
                                      ^ board.get_piece_bitboard(Piece::BlackKing));
        endgame = (non_pawn_count <= 2);
    }

    int legal_moves_searched = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        size_t best_i = i;
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (moves.scores[j] > moves.scores[best_i]) {
                best_i = j;
            }
        }
        std::swap(moves.moves[i], moves.moves[best_i]);
        std::swap(moves.scores[i], moves.scores[best_i]);
        Move m = moves.moves[i];

        if (!in_check) {
            // (1) Delta Pruning:
            if (!endgame && !m.isPromotion()) {
                int capture_val = get_capture_value(board, m);
                if (stand_pat + capture_val + g_delta_margin < alpha) {
                    continue;
                }
            }

            // (2) SEE Pruning:
            if (g_search_settings.see && see(board, m) < 0) {
                continue; // Prune losing captures
            }
        }

        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        legal_moves_searched++;

        int score = -quiescence(board, -beta, -alpha, ply + 1, info);
        board.unmakeMove(m, undo);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    // Checkmate/stalemate check inside quiescence search if we are in check and have no moves
    if (in_check && legal_moves_searched == 0) {
        return -MATE_SCORE + ply;
    }

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
int search_alphabeta(Board& board, int depth, int alpha, int beta, int ply, SearchInfo& info, Move excluded_move, MoveContext prev1, MoveContext prev2) {
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

    int raw_static_eval = 0;
    int static_eval = 0;
    if (!in_check) {
        raw_static_eval = evaluate(board);
        int corr = g_search_settings.corrhist ? get_correction_value(board, info) : 0;
        static_eval = raw_static_eval + corr;
    }

    if (ply < MAX_PLY) {
        info.eval_history[ply] = in_check ? -INFINITY_SCORE : static_eval;
    }

    bool improving = !in_check && ply >= 2 && info.eval_history[ply - 2] != -INFINITY_SCORE && static_eval > info.eval_history[ply - 2];

    // Reverse Futility Pruning (RFP) / Static Null Move Pruning
    if (excluded_move == MOVE_NONE && g_search_settings.rfp && depth <= 6 && !in_check && ply > 0) {
        int margin = depth * (improving ? 80 : 100);
        if (static_eval - margin >= beta) {
            return beta; // Fail high
        }
    }

    // Dynamic Null Move Pruning (NMP)
    if (excluded_move == MOVE_NONE && g_search_settings.nmp && depth >= 3 && !in_check && ply > 0) {
        Color us = board.get_side_to_move();
        Bitboard our_non_pawns = board.get_occupancy(us) 
                                ^ board.get_piece_bitboard(make_piece(us, PieceType::Pawn))
                                ^ board.get_piece_bitboard(make_piece(us, PieceType::King));
        if (our_non_pawns != EMPTY_BOARD && static_eval >= beta) {
            UndoState undo;
            board.makeNullMove(undo);
            
            // Dynamic reduction R based on depth, static eval surplus, and improving condition
            int R = 3 + (depth / 6) + std::min(3, (static_eval - beta) / 200) + (!improving ? 1 : 0);
            int score = -search_alphabeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, info, MOVE_NONE, {}, prev1);
            board.unmakeNullMove(undo);
            if (score >= beta) {
                return (std::abs(score) >= MATE_SCORE - MAX_PLY) ? beta : score;
            }
        }
    }

    // Base case: leaf node
    if (depth <= 0) {
        return quiescence(board, alpha, beta, ply, info);
    }

    bool pv_node = (beta - alpha > 1);

    Move tt_move = (tt_hit && excluded_move == MOVE_NONE) ? tt_entry.move : MOVE_NONE;

    // ProbCut (Probabilistic Cutoff):
    // At non-PV nodes with sufficient depth (depth >= 5), when not in check and away from mate bounds,
    // test if a tactical capture/promotion can quickly refute the position against an elevated beta window:
    // probCutBeta = beta + probCutMargin.
    // If a fast, shallow search at (depth - 4) beats probCutBeta, we verify it with a deeper search at (depth - 3).
    // If verification succeeds, we return probCutBeta as a fail-high cut immediately.
    //
    // Note on Interaction with Check Extensions:
    // - Check extensions prolong forced checking sequences to avoid tactical blunders or uncover deep mates.
    // - If ProbCut were applied carelessly to checking lines or near-mate scores, a shallow search (depth - 4)
    //   could easily miss opponent counter-check sequences or perpetual checks that a full check-extended search
    //   would resolve.
    // - To prevent this hazard, ProbCut is strictly gated by:
    //   1. Guarding against in-check positions (!in_check)
    //   2. Skipping near-mate score bounds (std::abs(beta) < MATE_SCORE - MAX_PLY)
    //   3. Skipping when a deep TT entry already proved the score falls below probCutBeta
    //   4. Requiring a 2-stage verification search (depth - 4 followed by depth - 3 verification) before cutting off.
    if (g_search_settings.probcut
        && !pv_node
        && depth >= PROBCUT_MIN_DEPTH
        && !in_check
        && excluded_move == MOVE_NONE
        && std::abs(beta) < MATE_SCORE - MAX_PLY)
    {
        int probcut_beta = beta + g_probcut_margin;
        bool skip_probcut = (tt_hit && tt_entry.depth >= depth - 3 && tt_entry.score < probcut_beta);

        if (!skip_probcut) {
            MoveList noisy_moves;
            generatePseudoLegalCaptures(board, noisy_moves);
            for (size_t i = 0; i < noisy_moves.size(); ++i) {
                noisy_moves.scores[i] = score_move(board, noisy_moves[i], tt_move, ply, info, prev1, prev2);
            }

            for (size_t i = 0; i < noisy_moves.size(); ++i) {
                size_t best_i = i;
                for (size_t j = i + 1; j < noisy_moves.size(); ++j) {
                    if (noisy_moves.scores[j] > noisy_moves.scores[best_i]) {
                        best_i = j;
                    }
                }
                std::swap(noisy_moves.moves[i], noisy_moves.moves[best_i]);
                std::swap(noisy_moves.scores[i], noisy_moves.scores[best_i]);
                Move m = noisy_moves.moves[i];

                Piece moved_p = board.get_piece(m.get_from());
                Square to_sq = m.get_to();

                UndoState undo;
                if (!board.makeMove(m, undo)) {
                    continue;
                }

                int score = -search_alphabeta(board, depth - 4, -probcut_beta, -probcut_beta + 1, ply + 1, info, MOVE_NONE, {moved_p, to_sq}, prev1);

                if (score >= probcut_beta) {
                    score = -search_alphabeta(board, depth - 3, -probcut_beta, -probcut_beta + 1, ply + 1, info, MOVE_NONE, {moved_p, to_sq}, prev1);
                }

                board.unmakeMove(m, undo);

                if (score >= probcut_beta) {
                    return probcut_beta;
                }
            }
        }
    }

    // Internal Iterative Reduction (IIR):
    // Reasoning difference between traditional IID and modern IIR:
    // - Internal Iterative Deepening (IID): When reaching a node at high depth with no TT move,
    //   older engines performed an extra shallow search (e.g., depth - 2) exclusively to populate
    //   the Transposition Table with a best move before conducting the full search. However, running
    //   this secondary search incurs considerable node overhead across the tree.
    // - Internal Iterative Reduction (IIR): Instead of spending nodes on an auxiliary search,
    //   IIR recognizes that lacking a TT move hurts move ordering efficiency, so it simply reduces
    //   the search depth of the current node by 1 ply (depth -= 1). This prunes tree growth when
    //   branching is uncertain. Once searched, the TT is populated at a lower cost, and any future
    //   passes in iterative deepening or re-searches will have a valid TT move to search at full depth.
    if (g_search_settings.iir 
        && excluded_move == MOVE_NONE 
        && depth >= IIR_MIN_DEPTH 
        && tt_move == MOVE_NONE) 
    {
        depth -= IIR_REDUCTION;
    }

    MoveList moves;
    generatePseudoLegalMoves(board, moves);

    // Score all pseudo-legal moves into moves.scores
    for (size_t i = 0; i < moves.size(); ++i) {
        moves.scores[i] = score_move(board, moves[i], tt_move, ply, info, prev1, prev2);
    }

    // Singular Extension (with Double Extension):
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

        int singular_score = search_alphabeta(board, singular_depth, singular_beta - 1, singular_beta, ply, info, tt_move, prev1, prev2);

        if (singular_score < singular_beta) {
            extension = 1;
            // Double singular extension when singular margin is doubled
            if (!pv_node && singular_score < singular_beta - singular_margin) {
                extension = 2;
            }
        }
    }

    int best_score = -INFINITY_SCORE;
    Move best_move = MOVE_NONE;

    bool futility_pruning = false;
    if (g_search_settings.futility && depth == 1 && !in_check && (static_eval + 150 < alpha)) {
        futility_pruning = true;
    }

    int moves_searched = 0;
    int quiet_moves_searched = 0;
    std::array<Move, 64> quiets_searched{};
    int quiets_count = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        // Selection sort to pick the best remaining move
        size_t best_i = i;
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (moves.scores[j] > moves.scores[best_i]) {
                best_i = j;
            }
        }
        std::swap(moves.moves[i], moves.moves[best_i]);
        std::swap(moves.scores[i], moves.scores[best_i]);
        Move m = moves.moves[i];
        if (m == excluded_move) {
            continue;
        }

        bool is_quiet = !m.isCapture() && !m.isPromotion();
        bool is_killer = (ply < MAX_PLY && (m == info.killer_moves[0][ply] || m == info.killer_moves[1][ply]));
        bool is_tt_move = (m == tt_move);

        if (futility_pruning && is_quiet && excluded_move == MOVE_NONE) {
            continue; // Prune quiet move
        }

        // Negative SEE Pruning: prune moves with losing tactical exchange at shallow depths
        if (g_search_settings.see
            && !pv_node
            && !in_check
            && depth <= 6
            && moves_searched > 0
            && excluded_move == MOVE_NONE)
        {
            if (is_quiet && !is_tt_move && !is_killer) {
                if (!see_ge(board, m, -30 * depth * depth)) {
                    continue;
                }
            } else if (m.isCapture() && !see_ge(board, m, -100 * depth)) {
                continue;
            }
        }

        Piece moved_p = board.get_piece(m.get_from());
        Square to_sq = m.get_to();

        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        g_tt.prefetch(board.get_hash_key());

        if (is_quiet && quiets_count < 64) {
            quiets_searched[quiets_count++] = m;
        }

        bool gives_check = is_in_check(board, board.get_side_to_move());

        // Late Move Pruning (LMP) / Move-count-based pruning:
        if (g_search_settings.lmp
            && is_quiet
            && !is_tt_move
            && !is_killer
            && !pv_node
            && !in_check
            && !gives_check
            && excluded_move == MOVE_NONE
            && depth <= g_lmp_max_depth)
        {
            int lmp_threshold = LMP_MOVE_THRESHOLDS[std::min(depth, 16)];
            if (quiet_moves_searched >= lmp_threshold) {
                board.unmakeMove(m, undo);
                continue;
            }
        }

        if (is_quiet) {
            quiet_moves_searched++;
        }

        moves_searched++;
        int ext = (m == tt_move) ? extension : 0;
        int new_depth = depth - 1 + ext;
        int score;

        MoveContext next_prev1 = {moved_p, to_sq};
        MoveContext next_prev2 = prev1;

        // Principal Variation Search (PVS) & Late Move Reductions (LMR)
        if (g_search_settings.pvs && moves_searched > 1) {
            if (g_search_settings.lmr && depth >= 3 && moves_searched > 3 && is_quiet && !in_check && !gives_check) {
                int reduction = g_lmr_table.table[std::min(depth, 63)][std::min(moves_searched, 63)];
                if (moved_p != Piece::None) {
                    int piece_idx = static_cast<int>(moved_p);
                    int sq_idx = static_cast<int>(to_sq);
                    reduction -= std::clamp(info.history_moves[piece_idx][sq_idx] / 4096, -2, 2);
                }
                if (!pv_node) reduction += 1;
                if (!improving) reduction += 1;
                if (is_killer || is_tt_move) reduction -= 1;
                if (info.thread_id > 0 && ((moves_searched + info.thread_id) % 3 == 0)) {
                    reduction += 1;
                }
                reduction = std::clamp(reduction, 1, std::max(1, new_depth - 1));

                score = -search_alphabeta(board, new_depth - reduction, -alpha - 1, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
                if (score > alpha && reduction > 1) {
                    score = -search_alphabeta(board, new_depth, -alpha - 1, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
                }
            } else {
                score = -search_alphabeta(board, new_depth, -alpha - 1, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
            }

            if (score > alpha && score < beta) {
                // Re-search with full window
                score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
            }
        } else {
            // Normal alpha-beta search
            if (g_search_settings.lmr && !g_search_settings.pvs && depth >= 3 && moves_searched > 3 && is_quiet && !in_check && !gives_check) {
                int reduction = g_lmr_table.table[std::min(depth, 63)][std::min(moves_searched, 63)];
                if (moved_p != Piece::None) {
                    int piece_idx = static_cast<int>(moved_p);
                    int sq_idx = static_cast<int>(to_sq);
                    reduction -= std::clamp(info.history_moves[piece_idx][sq_idx] / 4096, -2, 2);
                }
                if (!pv_node) reduction += 1;
                if (!improving) reduction += 1;
                if (is_killer || is_tt_move) reduction -= 1;
                if (info.thread_id > 0 && ((moves_searched + info.thread_id) % 3 == 0)) {
                    reduction += 1;
                }
                reduction = std::clamp(reduction, 1, std::max(1, new_depth - 1));

                score = -search_alphabeta(board, new_depth - reduction, -alpha - 1, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
                if (score > alpha) {
                    score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
                }
            } else {
                score = -search_alphabeta(board, new_depth, -beta, -alpha, ply + 1, info, MOVE_NONE, next_prev1, next_prev2);
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
            // Cutoff: Update move ordering heuristics
            if (excluded_move == MOVE_NONE) {
                int bonus = std::min(300, depth * depth);

                if (is_quiet) {
                    // Update killer moves
                    if (ply < MAX_PLY) {
                        info.killer_moves[1][ply] = info.killer_moves[0][ply];
                        info.killer_moves[0][ply] = m;
                    }

                    // Update countermoves
                    if (prev1.piece != Piece::None && prev1.to != Square::None) {
                        info.counter_moves[static_cast<int>(prev1.piece)][static_cast<int>(prev1.to)] = m;
                    }

                    // Positive update for cutoff quiet move
                    if (moved_p != Piece::None) {
                        int piece_idx = static_cast<int>(moved_p);
                        int sq_idx = static_cast<int>(to_sq);

                        update_corr_entry(info.history_moves[piece_idx][sq_idx], bonus);

                        if (prev1.piece != Piece::None && prev1.to != Square::None) {
                            int p1_idx = static_cast<int>(prev1.piece);
                            int to1_idx = static_cast<int>(prev1.to);
                            update_corr_entry(info.cont_history_1ply[p1_idx][to1_idx][piece_idx][sq_idx], bonus);
                        }

                        if (prev2.piece != Piece::None && prev2.to != Square::None) {
                            int p2_idx = static_cast<int>(prev2.piece);
                            int to2_idx = static_cast<int>(prev2.to);
                            update_corr_entry(info.cont_history_2ply[p2_idx][to2_idx][piece_idx][sq_idx], bonus);
                        }
                    }

                    // History Malus: penalize earlier quiet moves that failed to produce a cutoff
                    for (int qi = 0; qi < quiets_count; ++qi) {
                        Move qm = quiets_searched[qi];
                        if (qm == m) continue;

                        Piece q_p = board.get_piece(qm.get_from());
                        if (q_p != Piece::None) {
                            int q_p_idx = static_cast<int>(q_p);
                            int q_to_idx = static_cast<int>(qm.get_to());

                            update_corr_entry(info.history_moves[q_p_idx][q_to_idx], -bonus);

                            if (prev1.piece != Piece::None && prev1.to != Square::None) {
                                int p1_idx = static_cast<int>(prev1.piece);
                                int to1_idx = static_cast<int>(prev1.to);
                                update_corr_entry(info.cont_history_1ply[p1_idx][to1_idx][q_p_idx][q_to_idx], -bonus);
                            }

                            if (prev2.piece != Piece::None && prev2.to != Square::None) {
                                int p2_idx = static_cast<int>(prev2.piece);
                                int to2_idx = static_cast<int>(prev2.to);
                                update_corr_entry(info.cont_history_2ply[p2_idx][to2_idx][q_p_idx][q_to_idx], -bonus);
                            }
                        }
                    }
                } else if (m.isCapture()) {
                    // Update capture history
                    Piece victim = board.get_piece(to_sq);
                    PieceType victim_type = m.isEnPassant() ? PieceType::Pawn : ((victim != Piece::None) ? get_piece_type(victim) : PieceType::None);
                    if (moved_p != Piece::None && victim_type != PieceType::None) {
                        update_corr_entry(info.capture_history[static_cast<int>(moved_p)][static_cast<int>(to_sq)][static_cast<int>(victim_type)], bonus);
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
        if (in_check) {
            return -MATE_SCORE + ply; // Checkmate
        }
        return 0; // Stalemate
    }

    // Correction History Update:
    // When a search completes for a quiet position (not in check, not an excluded move search, away from mate scores),
    // update correction history towards (best_score - raw_static_eval) using the gravity formula.
    if (g_search_settings.corrhist
        && !in_check
        && excluded_move == MOVE_NONE
        && std::abs(best_score) < MATE_SCORE - MAX_PLY
        && (best_move == MOVE_NONE || (!best_move.isCapture() && !best_move.isPromotion())))
    {
        bool should_update = false;
        if (best_score >= beta && best_score > raw_static_eval) {
            should_update = true;
        } else if (best_score <= original_alpha && best_score < raw_static_eval) {
            should_update = true;
        } else if (best_score > original_alpha && best_score < beta) {
            should_update = true;
        }

        if (should_update) {
            int error = best_score - raw_static_eval;
            int bonus = std::clamp(error * depth, -CORRECTION_HISTORY_MAX, CORRECTION_HISTORY_MAX);

            Color stm = board.get_side_to_move();
            int stm_idx = (stm == Color::White) ? 0 : 1;
            uint64_t pawn_hash = compute_pawn_hash(board);
            uint64_t non_pawn_hash = compute_non_pawn_hash(board);

            update_corr_entry(info.pawn_corr_hist[stm_idx][pawn_hash % CORRECTION_HISTORY_SIZE], bonus);
            update_corr_entry(info.non_pawn_corr_hist[stm_idx][non_pawn_hash % CORRECTION_HISTORY_SIZE], bonus);
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

// Search root at specific depth with optional excluded moves list (for MultiPV)
SearchResult search_root(Board& board, int depth, SearchInfo& info, int alpha, int beta, const std::vector<Move>& excluded_root_moves) {
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

    // Exclude previously found best root moves (MultiPV exclusion list)
    if (!excluded_root_moves.empty()) {
        std::vector<Move> filtered_moves;
        filtered_moves.reserve(moves.size());
        for (Move m : moves) {
            bool excluded = false;
            for (Move ex : excluded_root_moves) {
                if (m == ex) {
                    excluded = true;
                    break;
                }
            }
            if (!excluded) {
                filtered_moves.push_back(m);
            }
        }
        moves = std::move(filtered_moves);
        if (moves.empty()) {
            return result;
        }
    }

    // Order moves, placing the PV move (best move from previous depth) or TT move first
    TTEntry tt_entry;
    Move tt_move = info.pv_move;
    info.tt_lookups++;
    if (g_tt.probe(board.get_hash_key(), 0, tt_entry) && tt_entry.move != MOVE_NONE) {
        info.tt_hits++;
        tt_move = tt_entry.move;
    }
    order_moves(board, moves, tt_move, 0, info, {}, {});

    info.pv_length[0] = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        Move m = moves[i];
        Piece moved_p = board.get_piece(m.get_from());
        Square to_sq = m.get_to();

        UndoState undo;
        if (!board.makeMove(m, undo)) {
            continue;
        }

        int score = 0;
        if (i == 0) {
            // Full window for first move
            score = -search_alphabeta(board, depth - 1, -beta, -alpha, 1, info, MOVE_NONE, {moved_p, to_sq}, {});
        } else {
            // PVS zero-window search for subsequent moves
            if (g_search_settings.pvs) {
                score = -search_alphabeta(board, depth - 1, -alpha - 1, -alpha, 1, info, MOVE_NONE, {moved_p, to_sq}, {});
                if (score > alpha && score < beta) {
                    score = -search_alphabeta(board, depth - 1, -beta, -alpha, 1, info, MOVE_NONE, {moved_p, to_sq}, {});
                }
            } else {
                score = -search_alphabeta(board, depth - 1, -beta, -alpha, 1, info, MOVE_NONE, {moved_p, to_sq}, {});
            }
        }
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

    // Populate PV line in result
    result.pv.clear();
    if (info.pv_length[0] > 0 && info.pv_table[0][0] == result.best_move) {
        for (int i = 0; i < info.pv_length[0]; ++i) {
            result.pv.push_back(info.pv_table[0][i]);
        }
    } else if (result.best_move != MOVE_NONE) {
        result.pv.push_back(result.best_move);
    }

    // Root node score is exact, record in TT only if no moves were excluded (primary PV)
    if (excluded_root_moves.empty() && result.best_move != MOVE_NONE) {
        g_tt.record(board.get_hash_key(), result.best_move, result.score, depth, TT_EXACT, 0);
    }

    return result;
}

SearchResult search_thread(Board& board, int max_depth, int thread_id) {
    auto info_ptr = std::make_unique<SearchInfo>();
    SearchInfo& info = *info_ptr;
    info.thread_id = thread_id;
    info.nodes_searched = 0;
    info.pv_move = MOVE_NONE;

    SearchResult final_result;
    int last_score = 0;
    Move prev_best_move = MOVE_NONE;
    int best_move_stable_count = 0;
    int dynamic_soft_limit = g_time_limit_soft_ms;

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
        int multipv_count = std::min(g_multipv, static_cast<int>(root_moves.size()));
        if (multipv_count <= 0) multipv_count = 1;

        std::vector<Move> excluded_root_moves;
        std::vector<SearchResult> pv_results;
        pv_results.reserve(multipv_count);

        for (int pv_idx = 0; pv_idx < multipv_count; ++pv_idx) {
            // Clear PV table for this PV pass
            std::fill(&info.pv_table[0][0], &info.pv_table[0][0] + MAX_PLY * MAX_PLY, MOVE_NONE);
            std::fill(&info.pv_length[0], &info.pv_length[0] + MAX_PLY, 0);

            SearchResult result;
            if (g_search_settings.aspiration && depth >= 5 && pv_idx == 0) {
                int base_delta = 50 + (thread_id % 4) * 15;
                int alpha = std::max(-INFINITY_SCORE, last_score - base_delta);
                int beta = std::min(INFINITY_SCORE, last_score + base_delta);
                int window = base_delta;
                
                while (true) {
                    result = search_root(board, depth, info, alpha, beta, excluded_root_moves);
                    if (g_stop_search.load()) {
                        break;
                    }
                    
                    if (result.score <= alpha) {
                        alpha = std::max(-INFINITY_SCORE, alpha - window);
                        window += window / 2;
                    } else if (result.score >= beta) {
                        beta = std::min(INFINITY_SCORE, beta + window);
                        window += window / 2;
                    } else {
                        break;
                    }
                }
            } else {
                result = search_root(board, depth, info, -INFINITY_SCORE, INFINITY_SCORE, excluded_root_moves);
            }

            if (g_stop_search.load()) {
                break;
            }

            if (result.best_move.is_none()) {
                break;
            }

            excluded_root_moves.push_back(result.best_move);
            pv_results.push_back(result);

            // Format and print UCI info string (only main thread prints)
            if (thread_id == 0) {
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - g_start_time)
                                       .count();
                std::cout << "info depth " << depth;
                
                if (g_multipv > 1) {
                    std::cout << " multipv " << (pv_idx + 1);
                }
                
                // Print score
                if (std::abs(result.score) > MATE_SCORE - MAX_PLY) {
                    int mate_in_plies = MATE_SCORE - std::abs(result.score);
                    int mate_in_moves = (mate_in_plies + 1) / 2;
                    std::cout << " score mate " << (result.score > 0 ? mate_in_moves : -mate_in_moves);
                } else {
                    std::cout << " score cp " << result.score;
                }

                double branching_factor = (depth > 0) ? std::pow(static_cast<double>(info.nodes_searched), 1.0 / depth) : 0.0;

                std::cout << " nodes " << info.nodes_searched
                          << " time " << elapsed_ms
                          << " nps " << (elapsed_ms > 0 ? (info.nodes_searched * 1000 / elapsed_ms) : 0)
                          << " hash_lookups " << info.tt_lookups
                          << " hash_hits " << info.tt_hits
                          << " bf " << branching_factor
                          << " pv";
                for (Move m : result.pv) {
                    std::cout << " " << m.to_string();
                }
                std::cout << std::endl;
            }
        }

        if (g_stop_search.load()) {
            break; // Discard partial/stopped results
        }

        if (pv_results.empty()) {
            if (depth == 1 && !final_result.best_move.is_none()) {
                // keep previous
            }
            break;
        }

        final_result = pv_results[0];
        info.pv_move = pv_results[0].best_move; // Save primary PV move for next iteration
        last_score = pv_results[0].score;

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
        final_result.pv = pv_results[0].pv;

        if (thread_id == 0) {
            // Early stop: checkmate found
            if (std::abs(final_result.score) > MATE_SCORE - MAX_PLY && g_multipv == 1) {
                break;
            }

            if (depth >= 2) {
                if (pv_results[0].best_move == prev_best_move) {
                    best_move_stable_count++;
                } else {
                    best_move_stable_count = 0;
                    if (g_time_limit_soft_ms != -1) {
                        int expanded = static_cast<int>(g_time_limit_soft_ms * 1.35);
                        if (g_time_limit_hard_ms != -1) {
                            dynamic_soft_limit = std::min(g_time_limit_hard_ms, expanded);
                        } else {
                            dynamic_soft_limit = expanded;
                        }
                    }
                }
            }
            prev_best_move = pv_results[0].best_move;

            // Soft time limit check: do not start next depth if soft limit exceeded
            if (dynamic_soft_limit != -1) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - g_start_time)
                                   .count();
                if (best_move_stable_count >= 5 && depth >= 7 && elapsed >= dynamic_soft_limit * 65 / 100) {
                    break;
                }
                if (elapsed >= dynamic_soft_limit) {
                    break;
                }
            }
        }
    }

    return final_result;
}

SearchResult search(Board& board, int max_depth) {
    g_tt.new_search();

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

    g_stop_search.store(false);

    return result;
}

} // namespace ChessEngine
