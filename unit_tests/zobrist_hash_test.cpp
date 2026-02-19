#include "zobrist_hash_test.h"

#include "../ChessBot/src/engine_core/board_state/zobrist_hash.h"
#include "../ChessBot/src/engine_core/board_state/pieces.h"

void ZobristHashTest::IdenticalPositionsShouldHaveSameHash() {
    ZobristHash::InitConstants();

    Pieces p1("8/8/8/8/8/8/pppppppp/PPPPPPPP");
    Pieces p2("8/8/8/8/8/8/pppppppp/PPPPPPPP");

    ZobristHash h1(p1, true, true, true, true, true);
    ZobristHash h2(p2, true, true, true, true, true);

    QCOMPARE(h1.GetValue(), h2.GetValue());
}

void ZobristHashTest::DifferentPositionsShouldHaveDifferentHash() {
    ZobristHash::InitConstants();

    Pieces p1("8/8/8/8/8/8/pppppppp/PPPPPPPP");
    Pieces p2("8/8/8/8/8/8/pppppppp/PPP1PPPP");  // убрали одну пешку

    ZobristHash h1(p1, true, true, true, true, true);
    ZobristHash h2(p2, true, true, true, true, true);

    QVERIFY(h1.GetValue() != h2.GetValue());
}

void ZobristHashTest::InvertPieceShouldBeReversible() {
    ZobristHash::InitConstants();

    Pieces p("8/8/8/8/8/8/pppppppp/PPPPPPPP");
    ZobristHash h(p, true, false, false, false, false);

    uint64_t before = h.GetValue();

    // Убираем и возвращаем пешку (white, a2 = square 8, type=0, side=0)
    h.InvertPiece(8, 0, 0);  // убрали
    h.InvertPiece(8, 0, 0);  // вернули

    QCOMPARE(h.GetValue(), before);
}

void ZobristHashTest::InvertMoveShouldFlipHash() {
    ZobristHash::InitConstants();

    Pieces p("8/8/8/8/8/8/pppppppp/PPPPPPPP");
    ZobristHash h(p, false, false, false, false, false);

    uint64_t original = h.GetValue();
    h.InvertMove(); // смена стороны
    QVERIFY(h.GetValue() != original);

    h.InvertMove(); // обратно
    QCOMPARE(h.GetValue(), original);
}

void ZobristHashTest::CastlingFlagsShouldAffectHash() {
    ZobristHash::InitConstants();

    Pieces p("8/8/8/8/8/8/pppppppp/PPPPPPPP");

    ZobristHash h1(p, false, false, false, false, false);
    ZobristHash h2(p, false, true, false, false, false);
    ZobristHash h3(p, false, true, true, false, false);

    QVERIFY(h1.GetValue() != h2.GetValue());
    QVERIFY(h2.GetValue() != h3.GetValue());
}
