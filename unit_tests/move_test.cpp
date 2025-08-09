#include "move_test.h"
#include "../ChessBot/board_state/move.h"

void MoveTest::DefaultMoveShouldHaveNoneFields() {
    Move m;

    QCOMPARE(m.GetFrom(), Move::None);
    QCOMPARE(m.GetTo(), Move::None);
    QCOMPARE(m.GetAttackerType(), Move::None);
    QCOMPARE(m.GetAttackerSide(), Move::None);
    QCOMPARE(m.GetDefenderType(), Move::None);
    QCOMPARE(m.GetDefenderSide(), Move::None);
    QCOMPARE(m.GetFlag(), Move::Flag::Default);
}

void MoveTest::MoveConstructorShouldStoreValues() {
    Move m(10, 20, 1, 0, 2, 1, Move::Flag::Capture);

    QCOMPARE(m.GetFrom(), 10);
    QCOMPARE(m.GetTo(), 20);
    QCOMPARE(m.GetAttackerType(), 1);
    QCOMPARE(m.GetAttackerSide(), 0);
    QCOMPARE(m.GetDefenderType(), 2);
    QCOMPARE(m.GetDefenderSide(), 1);
    QCOMPARE(m.GetFlag(), Move::Flag::Capture);
}

void MoveTest::FlagShouldBeSetAndRead() {
    Move m;
    m.SetFlag(Move::Flag::EnPassantCapture);
    QCOMPARE(m.GetFlag(), Move::Flag::EnPassantCapture);
}
