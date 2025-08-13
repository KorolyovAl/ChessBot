/************
* MoveOrdering — scores moves to improve search ordering.
* Uses simple promotion priority and MVV-LVA via board lookup.
************/
#pragma once

#include "../board_state/position.h"
#include "../board_state/move.h"

class MoveOrdering {
public:
    static int Score(const Move& move, const Pieces& pieces);
};
