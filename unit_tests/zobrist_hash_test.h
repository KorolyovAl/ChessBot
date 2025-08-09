#pragma once

#include <QObject>
#include <QtTest>

class ZobristHashTest : public QObject {
    Q_OBJECT

private slots:
    void IdenticalPositionsShouldHaveSameHash();
    void DifferentPositionsShouldHaveDifferentHash();
    void InvertPieceShouldBeReversible();
    void InvertMoveShouldFlipHash();
    void CastlingFlagsShouldAffectHash();
};
