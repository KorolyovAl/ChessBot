#pragma once

#include <QObject>
#include <QtTest>

class BitboardTest : public QObject {
    Q_OBJECT

private slots:
    void SetAndGetBit();
    void ResetBit();
    void CountOnes();
    void BitScanTests();
};
