#pragma once

#include <QObject>
#include <QtTest>

class PiecesTest : public QObject {
    Q_OBJECT

private slots:
    void EmptyBoardShouldHaveNoPieces();
    void SimpleFenShouldSetPawnsCorrectly();
    void SideAndAllAndEmptyShouldMatch();
    void ManualSetShouldAffectBoards();
    void StartingPositionShouldBeValid();
};
