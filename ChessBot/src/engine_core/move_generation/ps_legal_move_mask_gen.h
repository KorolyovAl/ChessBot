/**********************************************************
* ps_legal_move_mask_gen.h
*
* Helpers that build pseudo‑legal move masks (no check test)
* for every piece.  Used by move‑generation and threat checks.
**********************************************************/

#pragma once

#include "sliders_masks.h"
#include "../board_state/bitboard.h"
#include "../board_state/pieces.h"

class PsLegalMaskGen {
public:
    /* ───── Pawn ───── */
    static Bitboard PawnSinglePush(const Pieces& pcs, Side s);
    static Bitboard PawnDoublePush(const Pieces& pcs, Side s);
    static Bitboard PawnCaptureMask(const Pieces& pcs, Side s,
                                    bool include_all_attacks = false);

    /* ───── Pieces ──── */
    static Bitboard KnightMask(const Pieces& pcs, uint8_t sq, Side s,
                               bool only_captures = false);
    static Bitboard KingMask(const Pieces& pcs, uint8_t sq, Side s,
                             bool only_captures = false);
    static Bitboard BishopMask(const Pieces& pcs, uint8_t sq, Side s,
                               bool only_captures = false);
    static Bitboard RookMask(const Pieces& pcs, uint8_t sq, Side s,
                             bool only_captures = false);
    static Bitboard QueenMask(const Pieces& pcs, uint8_t sq, Side s,
                              bool only_captures = false);

    /* ───── Checks ──── */
    static bool SquareInDanger(const Pieces& pcs, uint8_t sq, Side s);

private:
    static Bitboard RayUntilBlock(const Pieces& pcs, uint8_t sq, Side s,
                                  bool only_captures,
                                  SlidersMasks::Direction dir, bool reverse);
};
