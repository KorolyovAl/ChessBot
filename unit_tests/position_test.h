#pragma once

#include <QObject>
#include <QtTest>

class PositionTest : public QObject {
    Q_OBJECT

private slots:
    // === Initialization & Basic Mechanics ===
    void ShouldInitializeCorrectlyFromFen();
    void ShouldAddAndRemovePieceCorrectly();
    void SideToMoveShouldSwitch();
    void CastlingFlagsShouldToggleCorrectly();

    // === Special Rules ===
    void EnPassantShouldSetAndGet();
    void PawnLongMoveShouldSetEnPassantCorrectly();
    void FiftyMoveCounterShouldResetAndIncrement();

    // === En Passant ===
    void MoveShouldHandleEnPassantCorrectly();

    // === Promotions ===
    void MoveShouldHandleAllPromotionsCorrectly();
    void MoveShouldHandleCaptureWithPromotionCorrectly();

    // === Standard Moves & Captures ===
    void MoveShouldUpdateBoardCorrectly();
    void MoveShouldHandleCaptureCorrectly();
    void MoveShouldHandleCastlingCorrectly();

    // === Repetition & Hash ===
    void ShouldDetectThreefoldRepetition();
    void HashShouldChangeOnPieceChanges();
    void HashShouldInvertOnEachMove();

    // === Edge & Invalid Cases ===
    void MoveShouldNotChangeBoardOnInvalidMove();
};
