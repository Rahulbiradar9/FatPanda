#pragma once

#include <cstdint>
#include <string>
#include "board/types.hpp"

namespace ChessEngine {

namespace MoveFlag {
    // 4-bit move flags (bits 12-15 of legacy data, mapped for compatibility)
    constexpr uint16_t NORMAL      = 0x0000;
    constexpr uint16_t DOUBLE_PUSH = 0x1000;
    constexpr uint16_t CASTLE_K    = 0x2000;
    constexpr uint16_t CASTLE_Q    = 0x3000;
    constexpr uint16_t CAPTURE     = 0x4000;
    constexpr uint16_t EN_PASSANT  = 0x5000;
    constexpr uint16_t PROMO_N     = 0x8000;
    constexpr uint16_t PROMO_B     = 0x9000;
    constexpr uint16_t PROMO_R     = 0xA000;
    constexpr uint16_t PROMO_Q     = 0xB000;
    constexpr uint16_t PROMO_N_CAP = 0xC000;
    constexpr uint16_t PROMO_B_CAP = 0xD000;
    constexpr uint16_t PROMO_R_CAP = 0xE000;
    constexpr uint16_t PROMO_Q_CAP = 0xF000;
}

class Move {
public:
    // Default constructor (null/none move)
    constexpr Move() : data_(0) {}

    // Construct move from raw data (32-bit)
    constexpr explicit Move(uint32_t data) : data_(data) {}

    // Construct move from squares, captured type, promotion type, and legacy flags
    constexpr Move(Square from, Square to, PieceType captured, PieceType promotion, uint16_t flag = MoveFlag::NORMAL) {
        uint32_t raw_data = static_cast<uint32_t>(from) | (static_cast<uint32_t>(to) << 6);

        // Map captured piece type (3 bits at 12-14)
        PieceType cap = captured;
        if (flag == MoveFlag::EN_PASSANT) {
            cap = PieceType::Pawn;
        } else if (flag == MoveFlag::CAPTURE || 
                   flag == MoveFlag::PROMO_N_CAP || 
                   flag == MoveFlag::PROMO_B_CAP || 
                   flag == MoveFlag::PROMO_R_CAP || 
                   flag == MoveFlag::PROMO_Q_CAP) {
            if (cap == PieceType::None) {
                cap = PieceType::Pawn; // default fallback for captures
            }
        }
        raw_data |= (static_cast<uint32_t>(cap) << 12);

        // Map promotion piece type (3 bits at 15-17)
        PieceType promo = promotion;
        if (promo == PieceType::None) {
            switch (flag & 0xF000) {
                case MoveFlag::PROMO_N:
                case MoveFlag::PROMO_N_CAP:
                    promo = PieceType::Knight;
                    break;
                case MoveFlag::PROMO_B:
                case MoveFlag::PROMO_B_CAP:
                    promo = PieceType::Bishop;
                    break;
                case MoveFlag::PROMO_R:
                case MoveFlag::PROMO_R_CAP:
                    promo = PieceType::Rook;
                    break;
                case MoveFlag::PROMO_Q:
                case MoveFlag::PROMO_Q_CAP:
                    promo = PieceType::Queen;
                    break;
                default:
                    break;
            }
        }
        raw_data |= (static_cast<uint32_t>(promo) << 15);

        // Map flags (4 bits at 18-21: 0 = Normal, 1 = Double Push, 2 = Castle King, 3 = Castle Queen, 4 = En Passant)
        uint32_t move_flag = 0;
        uint16_t legacy_flag_type = flag & 0xF000;
        if (legacy_flag_type == MoveFlag::DOUBLE_PUSH) {
            move_flag = 1;
        } else if (legacy_flag_type == MoveFlag::CASTLE_K) {
            move_flag = 2;
        } else if (legacy_flag_type == MoveFlag::CASTLE_Q) {
            move_flag = 3;
        } else if (legacy_flag_type == MoveFlag::EN_PASSANT) {
            move_flag = 4;
        }
        raw_data |= (move_flag << 18);

        data_ = raw_data;
    }

    // Construct move from squares and legacy flag
    constexpr Move(Square from, Square to, uint16_t flag = MoveFlag::NORMAL)
        : Move(from, to, PieceType::None, PieceType::None, flag) {}

    // CamelCase Inspectors (New Requirements)
    constexpr Square getSourceSquare() const {
        return static_cast<Square>(data_ & 0x3F);
    }

    constexpr Square getDestinationSquare() const {
        return static_cast<Square>((data_ >> 6) & 0x3F);
    }

    constexpr PieceType getCapturedPieceType() const {
        return static_cast<PieceType>((data_ >> 12) & 0x7);
    }

    constexpr PieceType getPromotionPieceType() const {
        return static_cast<PieceType>((data_ >> 15) & 0x7);
    }

    constexpr bool isCastling() const {
        uint32_t flag = (data_ >> 18) & 0xF;
        return flag == 2 || flag == 3;
    }

    constexpr bool isEnPassant() const {
        return ((data_ >> 18) & 0xF) == 4;
    }

    constexpr bool isDoublePawnPush() const {
        return ((data_ >> 18) & 0xF) == 1;
    }

    constexpr bool isCapture() const {
        return getCapturedPieceType() != PieceType::None;
    }

    constexpr bool isPromotion() const {
        return getPromotionPieceType() != PieceType::None;
    }

    // Legacy Accessors (Backward Compatibility)
    constexpr Square get_from() const { return getSourceSquare(); }
    constexpr Square get_to() const { return getDestinationSquare(); }
    constexpr bool is_none() const { return data_ == 0; }
    constexpr bool is_capture() const { return isCapture(); }
    constexpr bool is_promo() const { return isPromotion(); }
    constexpr bool is_double_push() const { return isDoublePawnPush(); }
    constexpr bool is_en_passant() const { return isEnPassant(); }
    
    constexpr bool is_castle_k() const {
        return ((data_ >> 18) & 0xF) == 2;
    }

    constexpr bool is_castle_q() const {
        return ((data_ >> 18) & 0xF) == 3;
    }

    constexpr bool is_castle() const {
        return isCastling();
    }

    constexpr PieceType get_promotion_piece_type() const {
        return getPromotionPieceType();
    }

    constexpr uint32_t get_raw() const {
        return data_;
    }

    constexpr uint16_t get_flags() const {
        uint32_t flag_val = (data_ >> 18) & 0xF;
        bool is_cap = isCapture();
        bool is_promo_flag = isPromotion();

        if (flag_val == 1) return MoveFlag::DOUBLE_PUSH;
        if (flag_val == 2) return MoveFlag::CASTLE_K;
        if (flag_val == 3) return MoveFlag::CASTLE_Q;
        if (flag_val == 4) return MoveFlag::EN_PASSANT;

        if (is_promo_flag) {
            PieceType p = getPromotionPieceType();
            if (is_cap) {
                if (p == PieceType::Knight) return MoveFlag::PROMO_N_CAP;
                if (p == PieceType::Bishop) return MoveFlag::PROMO_B_CAP;
                if (p == PieceType::Rook) return MoveFlag::PROMO_R_CAP;
                if (p == PieceType::Queen) return MoveFlag::PROMO_Q_CAP;
            } else {
                if (p == PieceType::Knight) return MoveFlag::PROMO_N;
                if (p == PieceType::Bishop) return MoveFlag::PROMO_B;
                if (p == PieceType::Rook) return MoveFlag::PROMO_R;
                if (p == PieceType::Queen) return MoveFlag::PROMO_Q;
            }
        }

        if (is_cap) return MoveFlag::CAPTURE;

        return MoveFlag::NORMAL;
    }

    // Operators
    constexpr bool operator==(const Move& other) const {
        return data_ == other.data_;
    }

    constexpr bool operator!=(const Move& other) const {
        return data_ != other.data_;
    }

    std::string to_string() const;
    std::string toString() const { return to_string(); }

private:
    uint32_t data_; 
};

constexpr Move MOVE_NONE = Move(0);

} // namespace ChessEngine
