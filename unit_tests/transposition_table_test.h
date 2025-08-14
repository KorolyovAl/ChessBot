/************
* TranspositionTable tests
* Checks: store-probe behavior for bounds and depth.
************/
#pragma once

#include <QObject>
#include <QtTest>

class TranspositionTableTest : public QObject {
    Q_OBJECT
private slots:
    void Probe_ShouldHitWithEnoughDepthAndWindow();
    void Probe_ShouldMissOnShallowDepthOrWrongWindow();
};
