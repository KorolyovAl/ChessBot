/************************************************
* Evaluation — static evaluation of a position.
*
* Material-centric score with tapered MG/EG terms.
* Positive scores favor White, negative — Black.
************************************************/
#pragma once

#include "../board_state/position.h"

class Evaluation {
public:
    // Main entry: static evaluation in centipawns.
    static int Evaluate(const Position& position);

private:
    // Baseline terms
    static int ComputeMaterialScore(const Pieces& pieces);
    static int ComputeBishopPairBonus(const Pieces& pieces);

    // Tapering (game phase) and PST
    static int ComputeGamePhase(const Pieces& pieces);
    static int ComputePieceSquareScoreMG(const Pieces& pieces);
    static int ComputePieceSquareScoreEG(const Pieces& pieces);

    // Pawn structure (MG/EG)
    static int ComputePawnStructureMG(const Pieces& pieces);
    static int ComputePawnStructureEG(const Pieces& pieces);

    // Mobility (MG/EG) and King safety (MG)
    static int ComputeMobilityMG(const Position& position);
    static int ComputeMobilityEG(const Position& position);
    static int ComputeKingSafetyMG(const Position& position);
};
