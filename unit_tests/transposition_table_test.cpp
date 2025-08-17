#include "transposition_table_test.h"

#include "../ChessBot/ai_logic/transposition_table.h"
#include "../ChessBot/board_state/move.h"
#include "../ChessBot/board_state/pieces.h"

static Move MakeQuiet(uint8_t from, uint8_t to) {
    return Move(from, to, static_cast<uint8_t>(PieceType::Knight),
                static_cast<uint8_t>(Side::White),
                Move::None, static_cast<uint8_t>(Side::Black),
                Move::Flag::Default);
}

void TranspositionTableTest::Probe_ShouldHitWithEnoughDepthAndWindow() {
    TranspositionTable tt(4);

    const uint64_t key = 0xABCDEF0123456789ull;
    const int depth = 6;
    const int score = 123;
    const Move best = MakeQuiet(1, 18);

    tt.Store(key, depth, score, TranspositionTable::Bound::Exact, best);

    int out_score = 0;
    Move out_best;
    const bool hit = tt.Probe(key, depth, score - 10, score + 10, out_score, out_best);

    QVERIFY(hit);
    QCOMPARE(out_score, score);
    QCOMPARE(out_best.GetFrom(), best.GetFrom());
    QCOMPARE(out_best.GetTo(), best.GetTo());
    QCOMPARE(static_cast<int>(out_best.GetFlag()), static_cast<int>(best.GetFlag()));
}

void TranspositionTableTest::Probe_ShouldMissOnShallowDepthOrWrongWindow() {
    TranspositionTable tt(4);

    const uint64_t key = 0x1111222233334444ull;
    const int depth = 4;
    const int score = 50;
    const Move best = MakeQuiet(8, 16);

    tt.Store(key, depth, score, TranspositionTable::Bound::Upper, best);

    {
        int out_score = 0; Move out_best;
        const bool hit = tt.Probe(key, depth - 1, score - 1000, score + 1000, out_score, out_best);
        QVERIFY(!hit); // not enough depth
    }
    {
        int out_score = 0; Move out_best;
        const bool hit = tt.Probe(key, depth, score - 1, score + 1000, out_score, out_best);
        QVERIFY(!hit); // window excludes upper-bound usefulness
    }
}
