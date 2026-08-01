#include "movegen.hpp"
#include "attacks.hpp"

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

std::vector<Move> generatePseudoLegalMoves(const Board& board) {
    std::vector<Move> moves;
    moves.reserve(100);

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
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q);
                } else {
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL);
                    // Double Push
                    if (rank == 1) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) + 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.emplace_back(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH);
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
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP);
                } else {
                    moves.emplace_back(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE);
                }
            }

            // En Passant
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.emplace_back(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT);
            }
        } else {
            // Black pawn pushes
            Square to_push = static_cast<Square>(static_cast<uint8_t>(from) - 8);
            if (!test_bit(both_occ, to_push)) {
                if (get_rank(to_push) == 0) {
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Knight, MoveFlag::PROMO_N);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Bishop, MoveFlag::PROMO_B);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Rook, MoveFlag::PROMO_R);
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::Queen, MoveFlag::PROMO_Q);
                } else {
                    moves.emplace_back(from, to_push, PieceType::None, PieceType::None, MoveFlag::NORMAL);
                    // Double Push
                    if (rank == 6) {
                        Square to_double = static_cast<Square>(static_cast<uint8_t>(from) - 16);
                        if (!test_bit(both_occ, to_double)) {
                            moves.emplace_back(from, to_double, PieceType::None, PieceType::None, MoveFlag::DOUBLE_PUSH);
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
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Knight, MoveFlag::PROMO_N_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Bishop, MoveFlag::PROMO_B_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Rook, MoveFlag::PROMO_R_CAP);
                    moves.emplace_back(from, to_cap, cap_type, PieceType::Queen, MoveFlag::PROMO_Q_CAP);
                } else {
                    moves.emplace_back(from, to_cap, cap_type, PieceType::None, MoveFlag::CAPTURE);
                }
            }

            // En Passant
            Square ep_sq = board.get_en_passant();
            if (ep_sq != Square::None && test_bit(attacks, ep_sq)) {
                moves.emplace_back(from, ep_sq, PieceType::Pawn, PieceType::None, MoveFlag::EN_PASSANT);
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
                moves.emplace_back(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE);
            } else {
                moves.emplace_back(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL);
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
                moves.emplace_back(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE);
            } else {
                moves.emplace_back(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL);
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
                moves.emplace_back(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE);
            } else {
                moves.emplace_back(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL);
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
                moves.emplace_back(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE);
            } else {
                moves.emplace_back(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL);
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
                moves.emplace_back(from, to, cap_type, PieceType::None, MoveFlag::CAPTURE);
            } else {
                moves.emplace_back(from, to, PieceType::None, PieceType::None, MoveFlag::NORMAL);
            }
        }

        // Castling rights (King must not be in check right now)
        if (!is_in_check(board, us)) {
            uint8_t rights = board.get_castling_rights();
            if (us == Color::White) {
                // White King-side
                if (rights & Castling::WK) {
                    if (!test_bit(both_occ, Square::F1) && !test_bit(both_occ, Square::G1)) {
                        if (!is_square_attacked(board, Square::F1, Color::Black) &&
                            !is_square_attacked(board, Square::G1, Color::Black)) {
                            moves.emplace_back(Square::E1, Square::G1, PieceType::None, PieceType::None, MoveFlag::CASTLE_K);
                        }
                    }
                }
                // White Queen-side
                if (rights & Castling::WQ) {
                    if (!test_bit(both_occ, Square::D1) && !test_bit(both_occ, Square::C1) && !test_bit(both_occ, Square::B1)) {
                        if (!is_square_attacked(board, Square::D1, Color::Black) &&
                            !is_square_attacked(board, Square::C1, Color::Black)) {
                            moves.emplace_back(Square::E1, Square::C1, PieceType::None, PieceType::None, MoveFlag::CASTLE_Q);
                        }
                    }
                }
            } else {
                // Black King-side
                if (rights & Castling::BK) {
                    if (!test_bit(both_occ, Square::F8) && !test_bit(both_occ, Square::G8)) {
                        if (!is_square_attacked(board, Square::F8, Color::White) &&
                            !is_square_attacked(board, Square::G8, Color::White)) {
                            moves.emplace_back(Square::E8, Square::G8, PieceType::None, PieceType::None, MoveFlag::CASTLE_K);
                        }
                    }
                }
                // Black Queen-side
                if (rights & Castling::BQ) {
                    if (!test_bit(both_occ, Square::D8) && !test_bit(both_occ, Square::C8) && !test_bit(both_occ, Square::B8)) {
                        if (!is_square_attacked(board, Square::D8, Color::White) &&
                            !is_square_attacked(board, Square::C8, Color::White)) {
                            moves.emplace_back(Square::E8, Square::C8, PieceType::None, PieceType::None, MoveFlag::CASTLE_Q);
                        }
                    }
                }
            }
        }
    }

    return moves;
}

std::vector<Move> generate_pseudo_legal_moves(const Board& board) {
    return generatePseudoLegalMoves(board);
}

std::vector<Move> generateLegalMoves(Board& board) {
    std::vector<Move> pseudo = generatePseudoLegalMoves(board);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());

    for (Move m : pseudo) {
        Board temp = board;
        if (temp.make_move(m)) {
            legal.push_back(m);
        }
    }

    return legal;
}

std::vector<Move> generate_legal_moves(Board& board) {
    return generateLegalMoves(board);
}

} // namespace ChessEngine
