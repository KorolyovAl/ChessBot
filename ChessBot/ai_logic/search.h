/************
* Search — iterative deepening alpha-beta with TT and quiescence.
************/
#pragma once

#include <cstdint>

#include "../board_state/position.h"
#include "../move_generation/move_list.h"
#include "transposition_table.h"

class Search {
public:
    struct Limits {
        int Depth = 4; // max depth in plies
    };

    static void SetHashMB(int megabytes);
    static void ClearTT();

    static Move FindBestMove(Position& position, const Limits& limits);

private:
    static constexpr int kInfinity = 32000;
    static constexpr int kMateScore = 30000;

    static int AlphaBeta(Position& position, int depth, int alpha, int beta, int ply);
    static int Quiescence(Position& position, int alpha, int beta, int ply);

    static bool IsTerminalDraw(const Position& position);

    static int MateIn(int ply);
    static int MatedIn(int ply);

    static TranspositionTable tt_;
};
