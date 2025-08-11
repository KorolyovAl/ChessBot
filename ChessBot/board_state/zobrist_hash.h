/************************************************
* ZobristHash — calculates and updates Zobrist hash values
* for a given chess position using pre-generated 64-bit keys.
*
* The hash reflects the current board state including pieces,
* castling rights, and side to move. Used in repetition detection.
*
* Contains:
* - Zobrist value
* - XOR update methods (invert piece, move side, castling)
* - Static Zobrist keys and one-time initialization
************************************************/

#pragma once

#include <cstdint>
#include <array>
#include <mutex>
#include "pieces.h"

class ZobristHash {
public:
    ZobristHash() = default;
    ZobristHash(Pieces pieces, bool black_to_move,
                bool white_long, bool white_short,
                bool black_long, bool black_short);

    friend bool operator==(ZobristHash left, ZobristHash right);
    friend bool operator<(ZobristHash left, ZobristHash right);

    void InvertPiece(uint8_t square, uint8_t type, uint8_t side);
    void InvertMove();
    void InvertWhiteLongCastling();
    void InvertWhiteShortCastling();
    void InvertBlackLongCastling();
    void InvertBlackShortCastling();
    void InvertEnPassantFile(uint8_t file);

    uint64_t GetValue() const;

    static void InitConstants();

private:
    uint64_t value_ = 0;

    static std::array<std::array<std::array<uint64_t, 6>, 2>, 64> piece_keys_;
    static std::array<uint64_t, 8> en_passant_file_keys_;
    static uint64_t black_to_move_key_;
    static uint64_t white_long_castling_key_;
    static uint64_t white_short_castling_key_;
    static uint64_t black_long_castling_key_;
    static uint64_t black_short_castling_key_;
    static std::once_flag init_flag_;
};
