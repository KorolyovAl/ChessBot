#pragma once

#include <QObject>
#include <QtTest>

class LegalMoveGenTest : public QObject {
    Q_OBJECT
private slots:
    // Базовая проверка целостности (apply/undo)
    void ApplyUndoRoundTrip();

    // Спец-ходы
    void EnPassantShouldAppearWhenAvailable();
    void CastlingMovesShouldAppearWhenLegal();
    void PromotionsShouldComeInPacksOfFour();

    // Перфты d1/d2 — небольшие, быстрые и однозначные
    void Perft_StartPos();
    void Perft_Kiwipete();
    void Perft_EnPassant();
    void Perft_Endgame();

    //void Perft_StartPos_Divide4_Print();
    void Debug_Divide_Position2_d4();
};
