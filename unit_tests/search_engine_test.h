/************
* SearchEngine tests
* Checks: PV legality, nodes limit adherence, TT score round-trip helper.
************/
#pragma once

#include <QObject>
#include <QtTest>

class SearchEngineTest : public QObject {
    Q_OBJECT
private slots:
    void PV_ShouldBeLegalSequence();
    void NodesLimit_ShouldBeRespected();
    void ScoreToTT_FromTT_ShouldRoundTrip();
};
