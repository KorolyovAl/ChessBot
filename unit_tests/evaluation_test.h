/************************************************
* Evaluation tests
*
* Notes:
* - We use board-only FEN. All other state is
*   passed via the Position constructor params.
* - Scores are expected within safe ranges to
*   avoid flakiness from future tuning.
************************************************/
#pragma once

#include <QObject>
#include <QtTest>

// Static evaluation tests.
// All scores are in centipawns from White's perspective.
class EvaluationTest : public QObject {
    Q_OBJECT
private slots:
    // Baseline sanity: tempo sign should be correct on the start position.
    void Tempo_ShouldBeAbout10();

    // PST sanity: a centralized knight must outscore a corner knight.
    void PST_KnightCenterBeatsCorner();

    // Bishop pair should give a clear positive swing for White.
    void BishopPair_ShouldBePositive();

    // Material should reflect a simple one-pawn imbalance (≈100cp),
    // allowing a small tolerance for PST/tempo.
    void Material_ShouldReflectPawnImbalance();

    // Passed pawn should be more valuable in endgame than in middlegame
    // (same position but without heavy pieces).
    void PassedPawn_BonusStrongerInEndgame();

    // King safety (MG): missing pawn shield / open columns near the king
    // should worsen evaluation for the side with the exposed king.
    void KingSafety_ShieldAndOpenColumnsMatter();

    // Micro-benchmark: cost of a single Evaluate() on a dense middlegame scene.
    // Uses QBENCHMARK so the time appears in the Test Results panel.
    void Benchmark_Evaluate_MidgameDense();
};
