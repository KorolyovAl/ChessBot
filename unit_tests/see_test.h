/************
* StaticExchangeEvaluation tests
* Checks: losing and winning captures; EP correctness on a crafted scene.
************/
#pragma once

#include <QObject>
#include <QtTest>

class SeeTest : public QObject {
    Q_OBJECT
private slots:
    void LosingCapture_ShouldBeNegative();
    void WinningCapture_ShouldBePositive();
};
