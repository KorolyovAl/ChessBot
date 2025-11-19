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

    void On_QxBf1_ShouldBeNegative();
    void Capture_QxBf1_ShouldBeNegative();
    void On_IllegalKingRecapture_Ignored();
    void Capture_EnPassant_BasicPositive();
};
