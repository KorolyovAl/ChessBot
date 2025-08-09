#include "bitboard_test.h"
#include "../ChessBot/board_state/bitboard.h"

void BitboardTest::SetAndGetBit() {
    Bitboard bb = 0;
    bb = BOp::Set_1(bb, 5);
    QVERIFY(BOp::GetBit(bb, 5));
    QVERIFY(!BOp::GetBit(bb, 4));
}

void BitboardTest::ResetBit() {
    Bitboard bb = BOp::Set_1(0, 10);
    bb = BOp::Set_0(bb, 10);
    QVERIFY(!BOp::GetBit(bb, 10));
}

void BitboardTest::CountOnes() {
    Bitboard bb = 0b101010;
    QCOMPARE(BOp::Count_1(bb), static_cast<uint8_t>(3));
}

void BitboardTest::BitScanTests() {
    Bitboard bb = 0;
    bb = BOp::Set_1(bb, 12);
    bb = BOp::Set_1(bb, 45);
    QCOMPARE(BOp::BitScanForward(bb), static_cast<uint8_t>(12));
    QCOMPARE(BOp::BitScanReverse(bb), static_cast<uint8_t>(45));
}
