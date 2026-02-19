/************************************************
* Move — represents a single chess move with full metadata.
* Used by move generation, AI, and game state logic to track all move properties.
* Includes support for special move types: castling, promotion, en passant, etc.

* Contains:
* - Origin and destination squares
* - Attacker and defender types and sides
* - Move flag enum (promotion, castling, etc.)
************************************************/

#pragma once

#include <cstdint>

class Move {
public:
    enum class Flag : uint8_t {
        Default,

        PawnLongMove,
        EnPassantCapture,

        WhiteLongCastling,
        WhiteShortCastling,
        BlackLongCastling,
        BlackShortCastling,

        PromoteToKnight,
        PromoteToBishop,
        PromoteToRook,
        PromoteToQueen,

        Capture
    };

    Move() = default;
    Move(uint8_t from, uint8_t to,
         uint8_t attacker_type, uint8_t attacker_side,
         uint8_t defender_type, uint8_t defender_side,
         Flag flag = Flag::Default);

    // Getters
    uint8_t GetFrom() const;
    uint8_t GetTo() const;
    uint8_t GetAttackerType() const;
    uint8_t GetAttackerSide() const;
    uint8_t GetDefenderType() const;
    uint8_t GetDefenderSide() const;
    Flag GetFlag() const;

    // Setters
    void SetFrom(uint8_t value);
    void SetTo(uint8_t value);
    void SetAttackerType(uint8_t value);
    void SetAttackerSide(uint8_t value);
    void SetDefenderType(uint8_t value);
    void SetDefenderSide(uint8_t value);
    void SetFlag(Flag value);

    static constexpr uint8_t None = 255;

private:
    uint8_t from_ = None;
    uint8_t to_ = None;
    uint8_t attacker_type_ = None;
    uint8_t attacker_side_ = None;
    uint8_t defender_type_ = None;
    uint8_t defender_side_ = None;
    Flag flag_ = Flag::Default;
};
