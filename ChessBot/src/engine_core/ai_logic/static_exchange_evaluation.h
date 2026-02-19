/************
* StaticExchangeEvaluation — material outcome of exchanges on a square.
* Provides evaluation for a specific capture and a general square probe.
************/
#pragma once

#include "../board_state/pieces.h"
#include "../board_state/move.h"

class StaticExchangeEvaluation {
public:
    // Net material (centipawns) for the side that makes the capture `move`.
    static int Capture(const Pieces& pieces, const Move& move);

    // Evaluate exchanges on `square` if `side_to_move` starts (approximate).
    static int On(const Pieces& pieces, uint8_t square, Side owner_square);
};
