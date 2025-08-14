#include "search_engine_test.h"

#include "../ChessBot/board_state/position.h"
#include "../ChessBot/move_generation/move_list.h"
#include "../ChessBot/move_generation/legal_move_gen.h"
#include "../ChessBot/ai_logic/search.h"

namespace {
    Position Make(const char* boardFEN, bool whiteToMove) {
        return Position(boardFEN, Position::NONE, true, true, true, true, whiteToMove ? 0 : 1);
    }

    bool ContainsMove(const MoveList& list, const Move& m) {
        for (uint32_t i = 0; i < list.GetSize(); ++i) {
            const Move& cur = list[i];
            if (cur.GetFrom() == m.GetFrom() && cur.GetTo() == m.GetTo() && cur.GetFlag() == m.GetFlag()) {
                return true;
            }
        }
        return false;
    }
} // namespace

void SearchEngineTest::PV_ShouldBeLegalSequence() {
    // Middlegame-like scene with enough moves to build a PV.
    Position pos = Make("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R", true);

    TranspositionTable tt(16);
    SearchEngine engine(tt);

    SearchLimits lim;
    lim.max_depth = 4;
    lim.nodes_limit = 0;

    const SearchResult res = engine.Search(pos, lim);
    QVERIFY2(res.depth >= 1, "Search depth should be >= 1");
    QVERIFY2(res.pv.length >= 1, "PV must contain at least one move");

    Position pvpos = pos;
    for (int i = 0; i < res.pv.length; ++i) {
        const Side stm = pvpos.IsWhiteToMove() ? Side::White : Side::Black;

        MoveList legal;
        LegalMoveGen::Generate(pvpos, stm, legal, false);

        const Move m = res.pv.moves[i];
        QVERIFY2(ContainsMove(legal, m), "PV move must be legal at its ply");

        Position::Undo u;
        pvpos.ApplyMove(m, u);
    }
}

void SearchEngineTest::NodesLimit_ShouldBeRespected() {
    Position pos = Make("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", true);

    TranspositionTable tt(8);
    SearchEngine engine(tt);

    SearchLimits lim;
    lim.max_depth = 64;
    lim.nodes_limit = 2000; // small cap to make the test fast

    const SearchResult res = engine.Search(pos, lim);

    QVERIFY2(res.nodes <= lim.nodes_limit, "Search must respect nodes_limit");
    QVERIFY2(res.depth >= 1, "Even with nodes cap, depth should be at least 1");
}

void SearchEngineTest::ScoreToTT_FromTT_ShouldRoundTrip() {
    // Non-mate values: the mapping should be identity per halfmove.
    const int values[] = { -900, -123, 0, 57, 320, 1534 };
    const int halfmove = 6;

    for (int v : values) {
        const int packed = SearchEngine::ScoreToTT(v, halfmove);
        const int unpack = SearchEngine::ScoreFromTT(packed, halfmove);
        QCOMPARE(unpack, v);
    }
}
