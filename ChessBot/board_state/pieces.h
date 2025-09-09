/************************************************
* Pieces — stores and manages all pieces for both players using separate bitboards.
* It provides aggregated masks for each side, occupied squares, and empty squares.
* Also includes a lightweight FEN-like string parser for initializing board state.

* Contains:
* - Bitboards per piece per side (6 × 2)
* - Aggregated side, all, and empty bitboards
* - Parser from simplified FEN
* - Comparison and board print functionality
************************************************/

#pragma once

#include <array>
#include <string>
#include "bitboard.h"

enum PieceType {
    Pawn = 0,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    Count,
    None
};

enum class Side {
    White = 0,
    Black = 1
};

class Pieces {
public:
    Pieces() = default;
    explicit Pieces(const std::string& short_fen);

    // Operators
    friend bool operator==(const Pieces& left, const Pieces& right);
    friend std::ostream& operator<<(std::ostream& os, const Pieces& pieces);

    // Setters
    void SetPieceBitboard(Side side, PieceType piece, Bitboard bb);

    // Getters
    Bitboard GetPieceBitboard(Side side, PieceType piece) const;
    Bitboard GetSideBoard(Side side) const;
    Bitboard GetInvSideBitboard(Side side) const;
    Bitboard GetAllBitboard() const;
    Bitboard GetEmptyBitboard() const;
    std::array<std::array<Bitboard, static_cast<int>(PieceType::Count)>, 2> GetPieceBitboards() const;

    // Utils
    void UpdateBitboard();
    static constexpr Side Inverse(Side side) {
        return side == Side::White ? Side::Black : Side::White;
    }

private:
    std::array<std::array<Bitboard, static_cast<int>(PieceType::Count)>, 2> piece_bitboards_{};
    std::array<Bitboard, 2> side_bitboards_{};
    std::array<Bitboard, 2> inv_side_bitboards_{};
    Bitboard all_{};
    Bitboard empty_{};
};
