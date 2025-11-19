/************
* LegalMoveGen is responsible for generating fully legal chess moves for a given side.
* It relies on pseudo-legal move masks and then filters out moves that leave the king in check.
* Pawn captures are handled separately to avoid file wrap issues and to leverage precomputed attack masks.
************/

#pragma once

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "move_list.h"

class LegalMoveGen {
public:
    // Fills 'out' with all legal moves for the given side.
    // If only_captures is true, generates only capture moves (including en passant).
    static void Generate(const Position& position, Side side, MoveList& out, bool only_captures = false);

private:
    // Converters from bit masks to moves for non-pawn pieces.
    static void PiecesMaskToMoves(const Pieces& pcs, Bitboard to_mask,
                                  uint8_t from_sq, PieceType attacker_type,
                                  Side attacker_side, MoveList& out);

    // Pawn moves: simple forward pushes and captures without using coordinate deltas.
    static void GenPawnCaptures (const Pieces& pcs, Side side, MoveList& out);
    static void GenPawnPushes  (const Pieces& pcs, Side side, MoveList& out);

    // Checks if the move is legal after applying it on a copy of Pieces.
    static bool IsLegalAfterMove(Pieces pcs, const Move& move);

    // Special moves: en passant and castling.
    static void AddEnPassantCaptures(const Pieces& pcs, Side side, uint8_t ep_square, MoveList& out);
    static void AddCastlingMoves   (const Pieces& pcs, Side side,
                                 bool long_castle, bool short_castle, MoveList& out);

    static inline void TryPushMove(const Pieces& pcs, MoveList& out,
                                   uint8_t from, uint8_t to,
                                   PieceType attacker_type, Side attacker_side,
                                   uint8_t defender_type, uint8_t defender_side,
                                   Move::Flag flag);
};
