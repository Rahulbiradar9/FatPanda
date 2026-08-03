#include "board.hpp"
#include "evaluation/nnue.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include "board/movegen.hpp"
#include "hash/zobrist.hpp"

namespace ChessEngine {

namespace {

// Helper to convert piece to ASCII character
char piece_to_char(Piece p) {
    switch (p) {
        case Piece::WhitePawn:   return 'P';
        case Piece::WhiteKnight: return 'N';
        case Piece::WhiteBishop: return 'B';
        case Piece::WhiteRook:   return 'R';
        case Piece::WhiteQueen:  return 'Q';
        case Piece::WhiteKing:   return 'K';
        case Piece::BlackPawn:   return 'p';
        case Piece::BlackKnight: return 'n';
        case Piece::BlackBishop: return 'b';
        case Piece::BlackRook:   return 'r';
        case Piece::BlackQueen:  return 'q';
        case Piece::BlackKing:   return 'k';
        default:                 return '.';
    }
}

// Helper to convert file index to char ('a'-'h')
char file_to_char(int file) {
    return static_cast<char>('a' + file);
}

// Helper to convert Square to coordinate string (e.g. "e4")
std::string square_to_string(Square sq) {
    if (sq == Square::None) return "-";
    int file = get_file(sq);
    int rank = get_rank(sq);
    std::string s;
    s += file_to_char(file);
    s += std::to_string(rank + 1);
    return s;
}

} // namespace

Board::Board() {
    initialize_zobrist_keys();
    clear();
}

void Board::clear() {
    // Clear all piece bitboards
    pieces_.fill(EMPTY_BOARD);

    // Clear occupancy bitboards
    occupancies_.fill(EMPTY_BOARD);

    // Reset squares array to empty
    board_squares_.fill(Piece::None);

    // Reset game state
    side_to_move_ = Color::White;
    castling_rights_ = Castling::NONE;
    en_passant_ = Square::None;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    hash_key_ = 0;
    history_len_ = 0;
    accum_history_.resize(1024);
    history_[history_len_++] = hash_key_;
    nnue_recompute_accumulator(*this, accum_history_[history_len_ - 1]);
}

void Board::set_piece(Square sq, Piece p) {
    if (!is_valid_square(sq)) return;

    size_t sq_idx = static_cast<size_t>(sq);
    Piece old_piece = board_squares_[sq_idx];

    // If the piece is already the same, do nothing
    if (old_piece == p) return;

    // Remove the old piece if present
    if (old_piece != Piece::None) {
        hash_key_ ^= piece_keys[static_cast<size_t>(old_piece)][sq_idx];
        size_t old_piece_idx = static_cast<size_t>(old_piece);
        clear_bit(pieces_[old_piece_idx], sq);

        Color old_color = get_piece_color(old_piece);
        size_t old_color_idx = static_cast<size_t>(old_color);
        clear_bit(occupancies_[old_color_idx], sq);
        clear_bit(occupancies_[2], sq); // both
    }

    // Add the new piece if not Piece::None
    if (p != Piece::None) {
        hash_key_ ^= piece_keys[static_cast<size_t>(p)][sq_idx];
        size_t new_piece_idx = static_cast<size_t>(p);
        set_bit(pieces_[new_piece_idx], sq);

        Color new_color = get_piece_color(p);
        size_t new_color_idx = static_cast<size_t>(new_color);
        set_bit(occupancies_[new_color_idx], sq);
        set_bit(occupancies_[2], sq); // both
    }

    // Update board array
    board_squares_[sq_idx] = p;
}

uint64_t Board::compute_hash_key() const {
    uint64_t key = 0;

    // Pieces
    for (int sq = 0; sq < 64; ++sq) {
        Piece p = get_piece(static_cast<Square>(sq));
        if (p != Piece::None) {
            key ^= piece_keys[static_cast<size_t>(p)][sq];
        }
    }

    // Castling rights
    key ^= castling_keys[castling_rights_];

    // En passant square
    if (en_passant_ != Square::None) {
        key ^= en_passant_keys[static_cast<size_t>(en_passant_)];
    }

    // Side to move
    if (side_to_move_ == Color::Black) {
        key ^= side_key;
    }

    return key;
}

void Board::reset_to_start() {
    clear();

    // 1. Setup White Pieces
    // Back rank (rank 1, index 0)
    set_piece(Square::A1, Piece::WhiteRook);
    set_piece(Square::B1, Piece::WhiteKnight);
    set_piece(Square::C1, Piece::WhiteBishop);
    set_piece(Square::D1, Piece::WhiteQueen);
    set_piece(Square::E1, Piece::WhiteKing);
    set_piece(Square::F1, Piece::WhiteBishop);
    set_piece(Square::G1, Piece::WhiteKnight);
    set_piece(Square::H1, Piece::WhiteRook);
    // Pawns (rank 2, index 1)
    for (int file = 0; file < 8; ++file) {
        set_piece(make_square(file, 1), Piece::WhitePawn);
    }

    // 2. Setup Black Pieces
    // Back rank (rank 8, index 7)
    set_piece(Square::A8, Piece::BlackRook);
    set_piece(Square::B8, Piece::BlackKnight);
    set_piece(Square::C8, Piece::BlackBishop);
    set_piece(Square::D8, Piece::BlackQueen);
    set_piece(Square::E8, Piece::BlackKing);
    set_piece(Square::F8, Piece::BlackBishop);
    set_piece(Square::G8, Piece::BlackKnight);
    set_piece(Square::H8, Piece::BlackRook);
    // Pawns (rank 7, index 6)
    for (int file = 0; file < 8; ++file) {
        set_piece(make_square(file, 6), Piece::BlackPawn);
    }

    // 3. Setup Game State
    side_to_move_ = Color::White;
    castling_rights_ = Castling::ALL;
    en_passant_ = Square::None;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    hash_key_ = compute_hash_key();
    history_len_ = 0;
    history_[history_len_++] = hash_key_;
    nnue_recompute_accumulator(*this, accum_history_[history_len_ - 1]);
}

void Board::print() const {
    std::cout << "\n  +---+---+---+---+---+---+---+---+\n";

    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            Square sq = make_square(file, rank);
            Piece p = get_piece(sq);
            std::cout << piece_to_char(p) << " | ";
        }
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    }

    std::cout << "    a   b   c   d   e   f   g   h\n\n";

    // Print other state info
    std::cout << "Side to move : " << (side_to_move_ == Color::White ? "White" : "Black") << "\n";
    
    std::cout << "Castling     : ";
    if (castling_rights_ == Castling::NONE) {
        std::cout << "-";
    } else {
        if (castling_rights_ & Castling::WK) std::cout << "K";
        if (castling_rights_ & Castling::WQ) std::cout << "Q";
        if (castling_rights_ & Castling::BK) std::cout << "k";
        if (castling_rights_ & Castling::BQ) std::cout << "q";
    }
    std::cout << "\n";

    std::cout << "En Passant   : " << square_to_string(en_passant_) << "\n";
    std::cout << "Halfmove     : " << halfmove_clock_ << "\n";
    std::cout << "Fullmove     : " << fullmove_number_ << "\n\n";
}

bool Board::make_move(Move m) {
    UndoState undo;
    return makeMove(m, undo);
}

void Board::setStartingPosition() {
    reset_to_start();
}

Piece Board::getPiece(Square sq) const {
    return get_piece(sq);
}

void Board::placePiece(Square sq, Piece p) {
    set_piece(sq, p);
    nnue_recompute_accumulator(*this, accum_history_[history_len_ - 1]);
}

void Board::removePiece(Square sq) {
    set_piece(sq, Piece::None);
    nnue_recompute_accumulator(*this, accum_history_[history_len_ - 1]);
}

void Board::movePiece(Square from, Square to) {
    Piece p = get_piece(from);
    set_piece(from, Piece::None);
    set_piece(to, p);
    nnue_recompute_accumulator(*this, accum_history_[history_len_ - 1]);
}

void Board::printBoard() const {
    print();
}

bool Board::loadFromFen(std::string_view fen) {
    return load_from_fen(fen);
}

std::string Board::toFen() const {
    return to_fen();
}

bool Board::makeMove(Move m, UndoState& undo) {
    // Save current hash in history
    if (history_len_ < 1024) {
        history_[history_len_++] = hash_key_;
    }

    // 1. Save state in UndoState
    undo.enPassant = en_passant_;
    undo.castlingRights = castling_rights_;
    undo.halfmoveClock = halfmove_clock_;
    undo.fullmoveNumber = fullmove_number_;
    undo.hashKey = hash_key_;

    // XOR out old ep and castling rights
    if (en_passant_ != Square::None) {
        hash_key_ ^= en_passant_keys[static_cast<size_t>(en_passant_)];
    }
    hash_key_ ^= castling_keys[castling_rights_];

    Square from = m.getSourceSquare();
    Square to = m.getDestinationSquare();
    Piece moving_piece = get_piece(from);
    Color us = side_to_move_;
    Color opponent = ~us;

    // Determine and save captured piece
    Piece captured = Piece::None;
    if (m.isCapture()) {
        if (m.isEnPassant()) {
            captured = (us == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        } else {
            captured = get_piece(to);
        }
    }
    undo.capturedPiece = captured;

    // Prepare NNUE incremental updates
    std::array<std::pair<Piece, Square>, 3> nnue_removed;
    std::array<std::pair<Piece, Square>, 3> nnue_added;
    int nnue_num_removed = 0;
    int nnue_num_added = 0;

    nnue_removed[nnue_num_removed++] = {moving_piece, from};

    if (m.isPromotion()) {
        Piece promo_piece = make_piece(us, m.getPromotionPieceType());
        nnue_added[nnue_num_added++] = {promo_piece, to};
    } else {
        nnue_added[nnue_num_added++] = {moving_piece, to};
    }

    if (m.isCapture()) {
        if (m.isEnPassant()) {
            Square cap_sq = make_square(get_file(to), get_rank(from));
            nnue_removed[nnue_num_removed++] = {captured, cap_sq};
        } else {
            nnue_removed[nnue_num_removed++] = {captured, to};
        }
    }

    if (m.isCastling()) {
        int file_diff = get_file(to) - get_file(from);
        Piece rook = make_piece(us, PieceType::Rook);
        Square r_from, r_to;
        if (file_diff > 0) { // Kingside
            r_from = (us == Color::White) ? Square::H1 : Square::H8;
            r_to = (us == Color::White) ? Square::F1 : Square::F8;
        } else { // Queenside
            r_from = (us == Color::White) ? Square::A1 : Square::A8;
            r_to = (us == Color::White) ? Square::D1 : Square::D8;
        }
        nnue_removed[nnue_num_removed++] = {rook, r_from};
        nnue_added[nnue_num_added++] = {rook, r_to};
    }

    if (history_len_ >= 2) {
        nnue_update_accumulator(
            accum_history_[history_len_ - 2],
            accum_history_[history_len_ - 1],
            nnue_removed, nnue_num_removed,
            nnue_added, nnue_num_added
        );
    }

    // Reset en-passant square for this move (might be set below for double pushes)
    en_passant_ = Square::None;

    // Increment clocks
    halfmove_clock_++;
    if (us == Color::Black) {
        fullmove_number_++;
    }

    // Reset halfmove clock if pawn moves or captures
    if (get_piece_type(moving_piece) == PieceType::Pawn || m.isCapture()) {
        halfmove_clock_ = 0;
    }

    // Handle captures
    if (m.isCapture()) {
        if (m.isEnPassant()) {
            Square cap_sq = make_square(get_file(to), get_rank(from));
            set_piece(cap_sq, Piece::None);
        } else {
            set_piece(to, Piece::None);
        }
    }

    // Handle promotion or normal move
    if (m.isPromotion()) {
        Piece promo_piece = make_piece(us, m.getPromotionPieceType());
        set_piece(from, Piece::None);
        set_piece(to, promo_piece);
    } else {
        set_piece(from, Piece::None);
        set_piece(to, moving_piece);
    }

    // Handle castling rook movement
    if (m.isCastling()) {
        int file_diff = get_file(to) - get_file(from);
        if (file_diff > 0) {
            // Kingside
            if (us == Color::White) {
                set_piece(Square::H1, Piece::None);
                set_piece(Square::F1, Piece::WhiteRook);
            } else {
                set_piece(Square::H8, Piece::None);
                set_piece(Square::F8, Piece::BlackRook);
            }
        } else {
            // Queenside
            if (us == Color::White) {
                set_piece(Square::A1, Piece::None);
                set_piece(Square::D1, Piece::WhiteRook);
            } else {
                set_piece(Square::A8, Piece::None);
                set_piece(Square::D8, Piece::BlackRook);
            }
        }
    }

    // Handle pawn double push
    if (m.isDoublePawnPush()) {
        int ep_rank = (us == Color::White) ? 2 : 5;
        en_passant_ = make_square(get_file(from), ep_rank);
    }

    // Update castling rights
    static constexpr std::array<uint8_t, 64> castling_mask = []() {
        std::array<uint8_t, 64> mask{};
        mask.fill(15);
        mask[static_cast<size_t>(Square::A1)] = 13; // ~WQ (15 - 2)
        mask[static_cast<size_t>(Square::H1)] = 14; // ~WK (15 - 1)
        mask[static_cast<size_t>(Square::E1)] = 12; // ~(WK | WQ) (15 - 3)
        mask[static_cast<size_t>(Square::A8)] = 7;  // ~BQ (15 - 8)
        mask[static_cast<size_t>(Square::H8)] = 11; // ~BK (15 - 4)
        mask[static_cast<size_t>(Square::E8)] = 3;  // ~(BK | BQ) (15 - 12)
        return mask;
    }();

    castling_rights_ &= (castling_mask[static_cast<size_t>(from)] & castling_mask[static_cast<size_t>(to)]);

    // Swap side to move
    side_to_move_ = opponent;

    // XOR in new ep, castling rights, and toggle side to move
    if (en_passant_ != Square::None) {
        hash_key_ ^= en_passant_keys[static_cast<size_t>(en_passant_)];
    }
    hash_key_ ^= castling_keys[castling_rights_];
    hash_key_ ^= side_key;

    // Check legality: King must not be left in check
    if (is_in_check(*this, us)) {
        unmakeMove(m, undo);
        return false;
    }

    return true;
}

void Board::unmakeMove(Move m, const UndoState& undo) {
    // 1. Swap side to move back
    side_to_move_ = ~side_to_move_;

    // 2. Restore basic game state variables
    en_passant_ = undo.enPassant;
    castling_rights_ = undo.castlingRights;
    halfmove_clock_ = undo.halfmoveClock;
    fullmove_number_ = undo.fullmoveNumber;

    Square from = m.getSourceSquare();
    Square to = m.getDestinationSquare();

    // 3. Move pieces back
    if (m.isPromotion()) {
        Piece pawn = make_piece(side_to_move_, PieceType::Pawn);
        set_piece(to, Piece::None);
        set_piece(from, pawn);
    } else {
        Piece moving_piece = get_piece(to);
        set_piece(to, Piece::None);
        set_piece(from, moving_piece);
    }

    // 4. Restore captured piece
    if (m.isCapture()) {
        if (m.isEnPassant()) {
            Square cap_sq = make_square(get_file(to), get_rank(from));
            set_piece(cap_sq, undo.capturedPiece);
        } else {
            set_piece(to, undo.capturedPiece);
        }
    }

    // 5. Restore castling rooks (if castling move)
    if (m.isCastling()) {
        int file_diff = get_file(to) - get_file(from);
        if (file_diff > 0) {
            // Kingside
            if (side_to_move_ == Color::White) {
                set_piece(Square::F1, Piece::None);
                set_piece(Square::H1, Piece::WhiteRook);
            } else {
                set_piece(Square::F8, Piece::None);
                set_piece(Square::H8, Piece::BlackRook);
            }
        } else {
            // Queenside
            if (side_to_move_ == Color::White) {
                set_piece(Square::D1, Piece::None);
                set_piece(Square::A1, Piece::WhiteRook);
            } else {
                set_piece(Square::D8, Piece::None);
                set_piece(Square::A8, Piece::BlackRook);
            }
        }
    }

    hash_key_ = undo.hashKey;

    if (history_len_ > 0) {
        history_len_--;
    }
}

bool Board::isRepetition() const {
    // Look back in the history at most by the halfmove_clock_
    int start = std::max(0, history_len_ - halfmove_clock_);
    for (int i = history_len_ - 1; i >= start; --i) {
        if (history_[i] == hash_key_) {
            return true;
        }
    }
    return false;
}

bool Board::is_insufficient_material() const {
    // If there are pawns, rooks, or queens, it is not insufficient material
    if (get_piece_bitboard(Piece::WhitePawn) || get_piece_bitboard(Piece::BlackPawn) ||
        get_piece_bitboard(Piece::WhiteRook) || get_piece_bitboard(Piece::BlackRook) ||
        get_piece_bitboard(Piece::WhiteQueen) || get_piece_bitboard(Piece::BlackQueen)) {
        return false;
    }

    int w_knights = count_bits(get_piece_bitboard(Piece::WhiteKnight));
    int b_knights = count_bits(get_piece_bitboard(Piece::BlackKnight));
    int w_bishops = count_bits(get_piece_bitboard(Piece::WhiteBishop));
    int b_bishops = count_bits(get_piece_bitboard(Piece::BlackBishop));

    int total_pieces = w_knights + b_knights + w_bishops + b_bishops;

    if (total_pieces == 0) {
        return true; // K vs K
    }

    if (total_pieces == 1) {
        // KB vs K or KN vs K
        return true;
    }

    if (total_pieces == 2 && w_bishops == 1 && b_bishops == 1) {
        // KB vs KB - check if bishops are on same square color
        Square w_bis_sq = get_lsb(get_piece_bitboard(Piece::WhiteBishop));
        Square b_bis_sq = get_lsb(get_piece_bitboard(Piece::BlackBishop));
        
        // A square is dark if (file + rank) % 2 == 0
        int w_color = (static_cast<int>(w_bis_sq) % 8 + static_cast<int>(w_bis_sq) / 8) % 2;
        int b_color = (static_cast<int>(b_bis_sq) % 8 + static_cast<int>(b_bis_sq) / 8) % 2;
        
        return w_color == b_color;
    }

    return false;
}

void Board::makeNullMove(UndoState& undo) {
    undo.enPassant = en_passant_;
    undo.castlingRights = castling_rights_;
    undo.halfmoveClock = halfmove_clock_;
    undo.fullmoveNumber = fullmove_number_;
    undo.hashKey = hash_key_;

    // Save enPassant square to hash and clear it
    if (en_passant_ != Square::None) {
        hash_key_ ^= en_passant_keys[static_cast<size_t>(en_passant_)];
        en_passant_ = Square::None;
    }
    if (side_to_move_ == Color::Black) {
        fullmove_number_++;
    }
    side_to_move_ = ~side_to_move_;
    hash_key_ ^= side_key;
    halfmove_clock_++;
    
    // Push the pre-null-move state to history
    if (history_len_ < 1024) {
        accum_history_[history_len_] = accum_history_[history_len_ - 1];
        history_[history_len_++] = hash_key_;
    }
}

void Board::unmakeNullMove(const UndoState& undo) {
    side_to_move_ = ~side_to_move_;
    en_passant_ = undo.enPassant;
    halfmove_clock_ = undo.halfmoveClock;
    fullmove_number_ = undo.fullmoveNumber;
    hash_key_ = undo.hashKey;
    if (history_len_ > 0) {
        history_len_--;
    }
}

} // namespace ChessEngine
