#include "movegen.hpp"
#include "attacks.hpp"
#include <algorithm>

namespace ChessEngine {

bool is_square_attacked(const Board& board, Square sq, Color attacker) {
    if (sq == Square::None || attacker == Color::None) return false;

    Color defender = ~attacker;
    Bitboard occupancy = board.get_occupancy(Color::None);

    // 1. Attack by Pawns
    Bitboard pawn_attackers = get_pawn_attacks(sq, defender);
    if (pawn_attackers & board.get_piece_bitboard(make_piece(attacker, PieceType::Pawn))) {
        return true;
    }

    // 2. Attack by Knights
    Bitboard knight_attackers = get_knight_attacks(sq);
    if (knight_attackers & board.get_piece_bitboard(make_piece(attacker, PieceType::Knight))) {
        return true;
    }

    // 3. Attack by King
    Bitboard king_attackers = get_king_attacks(sq);
    if (king_attackers & board.get_piece_bitboard(make_piece(attacker, PieceType::King))) {
        return true;
    }

    // 4. Attack by Bishops / Queens (sliding diagonal)
    Bitboard bishop_queen_attackers = get_bishop_attacks(sq, occupancy);
    Bitboard opponent_bishops_queens = board.get_piece_bitboard(make_piece(attacker, PieceType::Bishop)) |
                                        board.get_piece_bitboard(make_piece(attacker, PieceType::Queen));
    if (bishop_queen_attackers & opponent_bishops_queens) {
        return true;
    }

    // 5. Attack by Rooks / Queens (sliding orthogonal)
    Bitboard rook_queen_attackers = get_rook_attacks(sq, occupancy);
    Bitboard opponent_rooks_queens = board.get_piece_bitboard(make_piece(attacker, PieceType::Rook)) |
                                      board.get_piece_bitboard(make_piece(attacker, PieceType::Queen));
    if (rook_queen_attackers & opponent_rooks_queens) {
        return true;
    }

    return false;
}

bool is_in_check(const Board& board, Color color) {
    Bitboard king_bb = board.get_piece_bitboard(make_piece(color, PieceType::King));
    if (king_bb == EMPTY_BOARD) return false;
    Square king_sq = get_lsb(king_bb);
    return is_square_attacked(board, king_sq, ~color);
}

void generatePseudoLegalCaptures(const Board& board, MoveList& moves) {
    Color us = board.get_side_to_move();
    Color them = ~us;
    Bitboard their_occ = board.get_occupancy(them);
    Bitboard both_occ = board.get_occupancy(Color::None);

    // 1. Pawn captures & promotions
    Bitboard pawns = board.get_piece_bitboard(make_piece(us, PieceType::Pawn));
    while (pawns) {
        Square from = pop_lsb(pawns);
        if (us == Color::White) {
            // Queen promotions on push
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) + 8);
            if (!test_bit(both_occ, to_push) && get_rank(to_push) == 7) {
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B));
            }
            // Captures
            Bitboard attacks = get_pawn_attacks(from, Color::White);
            Bitboard targets = attacks & their_occ;
            while (targets) {
                Square to_cap = pop_lsb(targets);
                PieceType cap_type = get_piece_type(board.get_piece(to_cap));
                if (get_rank(to_cap) == 7) {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP));
                } else {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE));
                }
            }
            // En Passant
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.push_back(Move(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT));
            }
        } else {
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) - 8);
            if (!test_bit(both_occ, to_push) && get_rank(to_push) == 0) {
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R));
                moves.push_back(Move(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B));
            }
            Bitboard attacks = get_pawn_attacks(from, Color::Black);
            Bitboard targets = attacks & their_occ;
            while (targets) {
                Square to_cap = pop_lsb(targets);
                PieceType cap_type = get_piece_type(board.get_piece(to_cap));
                if (get_rank(to_cap) == 0) {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP));
                } else {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE));
                }
            }
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.push_back(Move(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT));
            }
        }
    }

    // 2. Knight captures
    Bitboard knights = board.get_piece_bitboard(make_piece(us, PieceType::Knight));
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard targets = get_knight_attacks(from) & their_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            PieceType cap_type = get_piece_type(board.get_piece(to));
            moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
        }
    }

    // 3. Bishop captures
    Bitboard bishops = board.get_piece_bitboard(make_piece(us, PieceType::Bishop));
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard targets = get_bishop_attacks(from, both_occ) & their_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            PieceType cap_type = get_piece_type(board.get_piece(to));
            moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
        }
    }

    // 4. Rook captures
    Bitboard rooks = board.get_piece_bitboard(make_piece(us, PieceType::Rook));
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard targets = get_rook_attacks(from, both_occ) & their_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            PieceType cap_type = get_piece_type(board.get_piece(to));
            moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
        }
    }

    // 5. Queen captures
    Bitboard queens = board.get_piece_bitboard(make_piece(us, PieceType::Queen));
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard targets = get_queen_attacks(from, both_occ) & their_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            PieceType cap_type = get_piece_type(board.get_piece(to));
            moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
        }
    }

    // 6. King captures
    Bitboard king = board.get_piece_bitboard(make_piece(us, PieceType::King));
    if (king) {
        Square from = pop_lsb(king);
        Bitboard targets = get_king_attacks(from) & their_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            PieceType cap_type = get_piece_type(board.get_piece(to));
            moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
        }
    }
}

void generatePseudoLegalQuiets(const Board& board, MoveList& moves) {
    Color us = board.get_side_to_move();
    Bitboard both_occ = board.get_occupancy(Color::None);
    Bitboard empty_sqs = ~both_occ;

    // 1. Pawn quiet pushes
    Bitboard pawns = board.get_piece_bitboard(make_piece(us, PieceType::Pawn));
    while (pawns) {
        Square from = pop_lsb(pawns);
        int rank = get_rank(from);

        if (us == Color::White) {
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) + 8);
            if (!test_bit(both_occ, to_push)) {
                if (get_rank(to_push) != 7) {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL));
                    if (rank == 1) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) + 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.push_back(Move(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH));
                        }
                    }
                }
            }
        } else {
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) - 8);
            if (!test_bit(both_occ, to_push)) {
                if (get_rank(to_push) != 0) {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL));
                    if (rank == 6) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) - 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.push_back(Move(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH));
                        }
                    }
                }
            }
        }
    }

    // 2. Knight quiet moves
    Bitboard knights = board.get_piece_bitboard(make_piece(us, PieceType::Knight));
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard targets = get_knight_attacks(from) & empty_sqs;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
        }
    }

    // 3. Bishop quiet moves
    Bitboard bishops = board.get_piece_bitboard(make_piece(us, PieceType::Bishop));
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard targets = get_bishop_attacks(from, both_occ) & empty_sqs;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
        }
    }

    // 4. Rook quiet moves
    Bitboard rooks = board.get_piece_bitboard(make_piece(us, PieceType::Rook));
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard targets = get_rook_attacks(from, both_occ) & empty_sqs;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
        }
    }

    // 5. Queen quiet moves
    Bitboard queens = board.get_piece_bitboard(make_piece(us, PieceType::Queen));
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard targets = get_queen_attacks(from, both_occ) & empty_sqs;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
        }
    }

    // 6. King quiet moves & Castling
    Bitboard king = board.get_piece_bitboard(make_piece(us, PieceType::King));
    if (king) {
        Square from = pop_lsb(king);
        Bitboard targets = get_king_attacks(from) & empty_sqs;
        while (targets) {
            Square to = pop_lsb(targets);
            moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
        }

        // Castling rights (King must not be in check right now)
        if (!is_in_check(board, us)) {
            uint8_t rights = board.get_castling_rights();
            int rank = (us == Color::White) ? 0 : 7;
            Color opponent = ~us;
            int k_from_file = get_file(from);

            // 1. King-side Castling
            uint8_t ks_flag = (us == Color::White) ? Castling::WK : Castling::BK;
            if (rights & ks_flag) {
                Square r_from = board.get_castling_rook(us, true);
                if (r_from != Square::None) {
                    int r_from_file = get_file(r_from);
                    int k_to_file = 6;
                    int r_to_file = 5;

                    int min_f = std::min({k_from_file, k_to_file, r_from_file, r_to_file});
                    int max_f = std::max({k_from_file, k_to_file, r_from_file, r_to_file});
                    bool path_clear = true;

                    for (int f = min_f; f <= max_f; ++f) {
                        Square sq = make_square(f, rank);
                        if (sq != from && sq != r_from) {
                            if (test_bit(both_occ, sq)) {
                                path_clear = false;
                                break;
                            }
                        }
                    }

                    if (path_clear) {
                        bool path_safe = true;
                        int step = (k_to_file > k_from_file) ? 1 : ((k_to_file < k_from_file) ? -1 : 0);
                        if (step != 0) {
                            for (int f = k_from_file + step; ; f += step) {
                                Square sq = make_square(f, rank);
                                if (is_square_attacked(board, sq, opponent)) {
                                    path_safe = false;
                                    break;
                                }
                                if (f == k_to_file) break;
                            }
                        }

                        if (path_safe) {
                            Square dest_sq = g_chess960 ? r_from : make_square(6, rank);
                            moves.push_back(Move(from, dest_sq, PieceType::None, PieceType::None, MoveFlag::CASTLE_K));
                        }
                    }
                }
            }

            // 2. Queen-side Castling
            uint8_t qs_flag = (us == Color::White) ? Castling::WQ : Castling::BQ;
            if (rights & qs_flag) {
                Square r_from = board.get_castling_rook(us, false);
                if (r_from != Square::None) {
                    int r_from_file = get_file(r_from);
                    int k_to_file = 2;
                    int r_to_file = 3;

                    int min_f = std::min({k_from_file, k_to_file, r_from_file, r_to_file});
                    int max_f = std::max({k_from_file, k_to_file, r_from_file, r_to_file});
                    bool path_clear = true;

                    for (int f = min_f; f <= max_f; ++f) {
                        Square sq = make_square(f, rank);
                        if (sq != from && sq != r_from) {
                            if (test_bit(both_occ, sq)) {
                                path_clear = false;
                                break;
                            }
                        }
                    }

                    if (path_clear) {
                        bool path_safe = true;
                        int step = (k_to_file > k_from_file) ? 1 : ((k_to_file < k_from_file) ? -1 : 0);
                        if (step != 0) {
                            for (int f = k_from_file + step; ; f += step) {
                                Square sq = make_square(f, rank);
                                if (is_square_attacked(board, sq, opponent)) {
                                    path_safe = false;
                                    break;
                                }
                                if (f == k_to_file) break;
                            }
                        }

                        if (path_safe) {
                            Square dest_sq = g_chess960 ? r_from : make_square(2, rank);
                            moves.push_back(Move(from, dest_sq, PieceType::None, PieceType::None, MoveFlag::CASTLE_Q));
                        }
                    }
                }
            }
        }
    }
}

void generatePseudoLegalMoves(const Board& board, MoveList& moves) {
    Color us = board.get_side_to_move();
    Color them = ~us;
    Bitboard our_occ = board.get_occupancy(us);
    Bitboard their_occ = board.get_occupancy(them);
    Bitboard both_occ = board.get_occupancy(Color::None);

    // --- 1. PAWN MOVES ---
    Bitboard pawns = board.get_piece_bitboard(make_piece(us, PieceType::Pawn));
    while (pawns) {
        Square from = pop_lsb(pawns);
        int rank = get_rank(from);

        if (us == Color::White) {
            // Single Push
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) + 8);
            if (!test_bit(both_occ, to_push)) {
                if (get_rank(to_push) == 7) {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q));
                } else {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL));
                    // Double Push
                    if (rank == 1) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) + 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.push_back(Move(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH));
                        }
                    }
                }
            }

            // Normal Pawn Captures & En-passant
            Bitboard attacks = get_pawn_attacks(from, Color::White);
            Bitboard capture_targets = attacks & their_occ;
            while (capture_targets) {
                Square to_cap = pop_lsb(capture_targets);
                PieceType cap_type = get_piece_type(board.get_piece(to_cap));
                if (get_rank(to_cap) == 7) {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP));
                } else {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE));
                }
            }

            // En Passant
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.push_back(Move(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT));
            }
        } else {
            // Black pawn pushes
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) - 8);
            if (!test_bit(both_occ, to_push)) {
                if (get_rank(to_push) == 0) {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R));
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q));
                } else {
                    moves.push_back(Move(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL));
                    // Double Push
                    if (rank == 6) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) - 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.push_back(Move(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH));
                        }
                    }
                }
            }

            // Normal Pawn Captures & En-passant
            Bitboard attacks = get_pawn_attacks(from, Color::Black);
            Bitboard capture_targets = attacks & their_occ;
            while (capture_targets) {
                Square to_cap = pop_lsb(capture_targets);
                PieceType cap_type = get_piece_type(board.get_piece(to_cap));
                if (get_rank(to_cap) == 0) {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP));
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP));
                } else {
                    moves.push_back(Move(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE));
                }
            }

            // En Passant
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.push_back(Move(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT));
            }
        }
    }

    // --- 2. KNIGHT MOVES ---
    Bitboard knights = board.get_piece_bitboard(make_piece(us, PieceType::Knight));
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard targets = get_knight_attacks(from) & ~our_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            if (test_bit(their_occ, to)) {
                PieceType cap_type = get_piece_type(board.get_piece(to));
                moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
            } else {
                moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
            }
        }
    }

    // --- 3. BISHOP MOVES ---
    Bitboard bishops = board.get_piece_bitboard(make_piece(us, PieceType::Bishop));
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard targets = get_bishop_attacks(from, both_occ) & ~our_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            if (test_bit(their_occ, to)) {
                PieceType cap_type = get_piece_type(board.get_piece(to));
                moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
            } else {
                moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
            }
        }
    }

    // --- 4. ROOK MOVES ---
    Bitboard rooks = board.get_piece_bitboard(make_piece(us, PieceType::Rook));
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard targets = get_rook_attacks(from, both_occ) & ~our_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            if (test_bit(their_occ, to)) {
                PieceType cap_type = get_piece_type(board.get_piece(to));
                moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
            } else {
                moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
            }
        }
    }

    // --- 5. QUEEN MOVES ---
    Bitboard queens = board.get_piece_bitboard(make_piece(us, PieceType::Queen));
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard targets = get_queen_attacks(from, both_occ) & ~our_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            if (test_bit(their_occ, to)) {
                PieceType cap_type = get_piece_type(board.get_piece(to));
                moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
            } else {
                moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
            }
        }
    }

    // --- 6. KING MOVES & CASTLING ---
    Bitboard king = board.get_piece_bitboard(make_piece(us, PieceType::King));
    if (king) {
        Square from = pop_lsb(king);
        Bitboard targets = get_king_attacks(from) & ~our_occ;
        while (targets) {
            Square to = pop_lsb(targets);
            if (test_bit(their_occ, to)) {
                PieceType cap_type = get_piece_type(board.get_piece(to));
                moves.push_back(Move(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE));
            } else {
                moves.push_back(Move(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL));
            }
        }

        // Castling rights (King must not be in check right now)
        if (!is_in_check(board, us)) {
            uint8_t rights = board.get_castling_rights();
            int rank = (us == Color::White) ? 0 : 7;
            Color opponent = ~us;
            int k_from_file = get_file(from);

            // 1. King-side Castling
            uint8_t ks_flag = (us == Color::White) ? Castling::WK : Castling::BK;
            if (rights & ks_flag) {
                Square r_from = board.get_castling_rook(us, true);
                if (r_from != Square::None) {
                    int r_from_file = get_file(r_from);
                    int k_to_file = 6;
                    int r_to_file = 5;

                    int min_f = std::min({k_from_file, k_to_file, r_from_file, r_to_file});
                    int max_f = std::max({k_from_file, k_to_file, r_from_file, r_to_file});
                    bool path_clear = true;

                    for (int f = min_f; f <= max_f; ++f) {
                        Square sq = make_square(f, rank);
                        if (sq != from && sq != r_from) {
                            if (test_bit(both_occ, sq)) {
                                path_clear = false;
                                break;
                            }
                        }
                    }

                    if (path_clear) {
                        bool path_safe = true;
                        int step = (k_to_file > k_from_file) ? 1 : ((k_to_file < k_from_file) ? -1 : 0);
                        if (step != 0) {
                            for (int f = k_from_file + step; ; f += step) {
                                Square sq = make_square(f, rank);
                                if (is_square_attacked(board, sq, opponent)) {
                                    path_safe = false;
                                    break;
                                }
                                if (f == k_to_file) break;
                            }
                        }

                        if (path_safe) {
                            Square dest_sq = g_chess960 ? r_from : make_square(6, rank);
                            moves.push_back(Move(from, dest_sq, PieceType::None, PieceType::None, MoveFlag::CASTLE_K));
                        }
                    }
                }
            }

            // 2. Queen-side Castling
            uint8_t qs_flag = (us == Color::White) ? Castling::WQ : Castling::BQ;
            if (rights & qs_flag) {
                Square r_from = board.get_castling_rook(us, false);
                if (r_from != Square::None) {
                    int r_from_file = get_file(r_from);
                    int k_to_file = 2;
                    int r_to_file = 3;

                    int min_f = std::min({k_from_file, k_to_file, r_from_file, r_to_file});
                    int max_f = std::max({k_from_file, k_to_file, r_from_file, r_to_file});
                    bool path_clear = true;

                    for (int f = min_f; f <= max_f; ++f) {
                        Square sq = make_square(f, rank);
                        if (sq != from && sq != r_from) {
                            if (test_bit(both_occ, sq)) {
                                path_clear = false;
                                break;
                            }
                        }
                    }

                    if (path_clear) {
                        bool path_safe = true;
                        int step = (k_to_file > k_from_file) ? 1 : ((k_to_file < k_from_file) ? -1 : 0);
                        if (step != 0) {
                            for (int f = k_from_file + step; ; f += step) {
                                Square sq = make_square(f, rank);
                                if (is_square_attacked(board, sq, opponent)) {
                                    path_safe = false;
                                    break;
                                }
                                if (f == k_to_file) break;
                            }
                        }

                        if (path_safe) {
                            Square dest_sq = g_chess960 ? r_from : make_square(2, rank);
                            moves.push_back(Move(from, dest_sq, PieceType::None, PieceType::None, MoveFlag::CASTLE_Q));
                        }
                    }
                }
            }
        }
    }
}

std::vector<Move> generatePseudoLegalMoves(const Board& board) {
    MoveList list;
    generatePseudoLegalMoves(board, list);
    return std::vector<Move>(list.moves.begin(), list.moves.begin() + list.size());
}

std::vector<Move> generate_pseudo_legal_moves(const Board& board) {
    return generatePseudoLegalMoves(board);
}

std::vector<Move> generateLegalMoves(Board& board) {
    MoveList pseudo;
    generatePseudoLegalMoves(board, pseudo);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());

    for (size_t i = 0; i < pseudo.size(); ++i) {
        UndoState undo;
        if (board.makeMove(pseudo[i], undo)) {
            legal.push_back(pseudo[i]);
            board.unmakeMove(pseudo[i], undo);
        }
    }

    return legal;
}

bool is_pseudo_legal(const Board& board, Move m) {
    if (m.is_none()) return false;
    Square from = m.getSourceSquare();
    Square to = m.getDestinationSquare();
    Piece p = board.get_piece(from);
    if (p == Piece::None) return false;
    Color us = board.get_side_to_move();
    if (get_piece_color(p) != us) return false;

    Piece target = board.get_piece(to);
    Color target_color = (target != Piece::None) ? get_piece_color(target) : Color::None;
    Bitboard occ = board.get_occupancy(Color::None);
    PieceType pt = get_piece_type(p);

    if (m.isCastling()) {
        MoveList quiets;
        generatePseudoLegalQuiets(board, quiets);
        for (size_t i = 0; i < quiets.size(); ++i) {
            if (quiets[i] == m) return true;
        }
        return false;
    }

    if (target_color == us) return false;

    // A quiet move cannot land on an occupied square
    if (!m.isCapture() && target != Piece::None) return false;
    // A capture move cannot land on an empty square unless en-passant
    if (m.isCapture() && !m.isEnPassant() && target == Piece::None) return false;

    if (pt == PieceType::Pawn) {
        if (m.isEnPassant()) {
            return to == board.get_en_passant();
        }
        int from_rank = get_rank(from);
        int to_rank = get_rank(to);
        int from_file = get_file(from);
        int to_file = get_file(to);
        int dir = (us == Color::White) ? 1 : -1;

        if (from_file == to_file) {
            if (target != Piece::None) return false;
            if (to_rank == from_rank + dir) return true;
            int start_rank = (us == Color::White) ? 1 : 6;
            Square skipped = (us == Color::White) ? static_cast<Square>(static_cast<int>(from) + 8)
                                                  : static_cast<Square>(static_cast<int>(from) - 8);
            if (from_rank == start_rank && to_rank == from_rank + 2 * dir && board.get_piece(skipped) == Piece::None) {
                return true;
            }
            return false;
        } else if (std::abs(from_file - to_file) == 1 && to_rank == from_rank + dir) {
            return target != Piece::None && target_color != us;
        }
        return false;
    } else if (pt == PieceType::Knight) {
        return (get_knight_attacks(from) & (1ULL << static_cast<int>(to))) != 0;
    } else if (pt == PieceType::Bishop) {
        return (get_bishop_attacks(from, occ) & (1ULL << static_cast<int>(to))) != 0;
    } else if (pt == PieceType::Rook) {
        return (get_rook_attacks(from, occ) & (1ULL << static_cast<int>(to))) != 0;
    } else if (pt == PieceType::Queen) {
        return (get_queen_attacks(from, occ) & (1ULL << static_cast<int>(to))) != 0;
    } else if (pt == PieceType::King) {
        return (get_king_attacks(from) & (1ULL << static_cast<int>(to))) != 0;
    }
    return false;
}

std::vector<Move> generate_legal_moves(Board& board) {
    return generateLegalMoves(board);
}

} // namespace ChessEngine
