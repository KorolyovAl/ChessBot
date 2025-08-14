/************
* MoveOrdering tests
* Checks: TT move top priority; captures outrank quiet moves.
************/
#pragma once

#include <QObject>
#include <QtTest>

class MoveOrderingTest : public QObject {
    Q_OBJECT
private slots:
    void TtMove_ShouldOutscoreOthers();
    void Capture_ShouldOutscoreQuiet();
};
