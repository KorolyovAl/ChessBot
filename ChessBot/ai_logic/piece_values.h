/************
* Canonical piece values definition.
************/
#pragma once

#include "../board_state/pieces.h"

namespace EvalValues {
    const int kPieceValueCp[PieceType::Count] = {
        100,  // Pawn
        320,  // Knight
        330,  // Bishop
        500,  // Rook
        900,  // Queen
        0     // King (material value not used; SEE overrides)
    };
}
