/************************************************
* Position — central representation of the current board state.
*
* Tracks piece placement, castling rights, en passant, move counters,
* repetition history, and Zobrist hash. Handles move execution and updates.
*
* Contains:
* - Pieces and ZobristHash
* - Castling flags, en passant square
* - Side to move, 50-move rule counter, repetition tracker
************************************************/

#pragma once

#include "pieces.h"
#include "move.h"
#include "zobrist_hash.h"
#include "repetition_history.h"

class Position {
public:
    Position() = default;

    Position(const std::string& short_fen, uint8_t en_passant,
             bool white_long, bool white_short,
             bool black_long, bool black_short,
             float move_counter);

    void ApplyMove(Move move);

    // Read-only accessors
    const Pieces& GetPieces() const;
    const ZobristHash& GetHash() const;

    uint8_t GetEnPassantSquare() const;
    bool GetWhiteLongCastling() const;
    bool GetWhiteShortCastling() const;
    bool GetBlackLongCastling() const;
    bool GetBlackShortCastling() const;

    bool IsWhiteToMove() const;
    bool IsFiftyMoveRuleDraw() const;
    bool IsThreefoldRepetition() const;

    friend std::ostream& operator<<(std::ostream& os, const Position& position);

    static constexpr uint8_t NONE = 255;

private:
    friend class PositionTest;

    void AddPiece(uint8_t square, uint8_t type, uint8_t side);
    void RemovePiece(uint8_t square, uint8_t type, uint8_t side);
    void SetEnPassantSquare(uint8_t square);
    void DisableCastling(Side side, bool long_castle);

    void UpdateMoveCounter();
    void UpdateFiftyMovesCounter(bool breaking_event);

    Pieces pieces_;
    uint8_t en_passant_ = NONE;

    bool white_long_castling_ = false;
    bool white_short_castling_ = false;
    bool black_long_castling_ = false;
    bool black_short_castling_ = false;

    uint16_t  move_counter_ = 0;
    uint8_t fifty_move_counter_ = 0;

    ZobristHash hash_;
    RepetitionHistory repetition_history_;
};
