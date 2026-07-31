#pragma once

#include <cstdint>
#include <string>
#include "board/types.hpp"

namespace ChessEngine {

namespace MoveFlag {
    // 4-bit move flags (bits 12-15)
    constexpr uint16_t NORMAL      = 0x0000;
    constexpr uint16_t DOUBLE_PUSH = 0x1000; // Double pawn push
    constexpr uint16_t CASTLE_K    = 0x2000; // King-side castling
    constexpr uint16_t CASTLE_Q    = 0x3000; // Queen-side castling
    constexpr uint16_t CAPTURE     = 0x4000; // Regular capture
    constexpr uint16_t EN_PASSANT  = 0x5000; // En-passant capture
    constexpr uint16_t PROMO_N     = 0x8000; // Knight promotion
    constexpr uint16_t PROMO_B     = 0x9000; // Bishop promotion
    constexpr uint16_t PROMO_R     = 0xA000; // Rook promotion
    constexpr uint16_t PROMO_Q     = 0xB000; // Queen promotion
    constexpr uint16_t PROMO_N_CAP = 0xC000; // Knight promotion + capture
    constexpr uint16_t PROMO_B_CAP = 0xD000; // Bishop promotion + capture
    constexpr uint16_t PROMO_R_CAP = 0xE000; // Rook promotion + capture
    constexpr uint16_t PROMO_Q_CAP = 0xF000; // Queen promotion + capture
}

class Move {
public:
    // Default constructor (null/none move)
    constexpr Move() : data_(0) {}

    // Construct move from raw data
    constexpr explicit Move(uint16_t data) : data_(data) {}

    // Construct move from squares and flags
    constexpr Move(Square from, Square to, uint16_t flag = MoveFlag::NORMAL)
        : data_(static_cast<uint16_t>(from) | (static_cast<uint16_t>(to) << 6) | flag) {}

    // Accessors
    constexpr Square get_from() const {
        return static_cast<Square>(data_ & 0x3F);
    }

    constexpr Square get_to() const {
        return static_cast<Square>((data_ >> 6) & 0x3F);
    }

    constexpr uint16_t get_flags() const {
        return data_ & 0xF000;
    }

    constexpr uint16_t get_raw() const {
        return data_;
    }

    // Type detection helpers
    constexpr bool is_none() const {
        return data_ == 0;
    }

    constexpr bool is_capture() const {
        return (data_ & 0x4000) != 0;
    }

    constexpr bool is_promo() const {
        return (data_ & 0x8000) != 0;
    }

    constexpr bool is_double_push() const {
        return (data_ & 0xF000) == MoveFlag::DOUBLE_PUSH;
    }

    constexpr bool is_en_passant() const {
        return (data_ & 0xF000) == MoveFlag::EN_PASSANT;
    }

    constexpr bool is_castle_k() const {
        return (data_ & 0xF000) == MoveFlag::CASTLE_K;
    }

    constexpr bool is_castle_q() const {
        return (data_ & 0xF000) == MoveFlag::CASTLE_Q;
    }

    constexpr bool is_castle() const {
        uint16_t flag = data_ & 0xF000;
        return flag == MoveFlag::CASTLE_K || flag == MoveFlag::CASTLE_Q;
    }

    // Extract the PieceType of promotion target
    constexpr PieceType get_promotion_piece_type() const {
        if (!is_promo()) return PieceType::None;
        
        // Extract flags representing promo piece type (bits 12 and 13)
        // Bit 15 is Promotion, Bit 14 is Capture
        uint16_t promo_bits = data_ & 0x3000;
        switch (promo_bits) {
            case 0x0000: return PieceType::Knight;
            case 0x1000: return PieceType::Bishop;
            case 0x2000: return PieceType::Rook;
            case 0x3000: return PieceType::Queen;
            default:     return PieceType::None;
        }
    }

    // Operators
    constexpr bool operator==(const Move& other) const {
        return data_ == other.data_;
    }

    constexpr bool operator!=(const Move& other) const {
        return data_ != other.data_;
    }

    // Convert to UCI standard string format (e.g. "e2e4", "e7e8q")
    std::string to_string() const;

private:
    uint16_t data_; // bit 0-5: from square, bit 6-11: to square, bit 12-15: flags
};

constexpr Move MOVE_NONE = Move(0);

} // namespace ChessEngine
