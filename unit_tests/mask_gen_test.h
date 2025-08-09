#pragma once

#include <QObject>
#include <QtTest>

class MaskGenTest : public QObject {
    Q_OBJECT
private slots:
    void PawnAttackMaskShouldMatchExpected();

    void RookRayShouldStopBeforeOwnBlocker();

    void BishopRayShouldIncludeCaptureSquare();
    void SquareInDangerShouldDetectPawnCheck();

    void RookRayShouldIncludeCaptureSquare();
    void KnightMaskOnlyCapturesShouldFilter();
    void PawnDoublePushShouldRespectBlockers();
    void PawnCaptureMaskAllAttacksVsLegal();
    void BishopMaskOnEmptyBoardEqualsTable();
    void SquareInDangerShouldReturnFalseWhenSafe();
};
