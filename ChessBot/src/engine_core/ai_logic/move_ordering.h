/*
 * Move ordering — context and scoring API.
 * Contents:
 *   - struct Context: tt_move, cutoff1, cutoff2, history, side_to_move.
 *   - Score(...): returns scalar priority for sorting moves (higher is better).
 * Scoring order: TT move → promotions/EP → captures (MVV-LVA + SEE) → CutoffMoves → History → SimpleMoves.
 */

#pragma once

#include "../board_state/pieces.h"
#include "../board_state/move.h"

class MoveOrdering {
public:
    struct Context {
        Move tt_move;
        uint16_t cutoff1;
        uint16_t cutoff2;
        const int (*history)[2][64][64];
        Side side_to_move;
    };

    static int Score(const Move& move, const Pieces& pieces, const Context& ctx);
};
