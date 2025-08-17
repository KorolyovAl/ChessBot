#include "legal_move_gen_test.h"

#include "../ChessBot/board_state/position.h"
#include "../ChessBot/board_state/move.h"
#include "../ChessBot/board_state/pieces.h"
#include "../ChessBot/move_generation/move_list.h"
#include "../ChessBot/move_generation/legal_move_gen.h"
#include "../ChessBot/move_generation/ps_legal_move_mask_gen.h"

namespace {
    // side helper
    inline Side Opp(Side s) {
        return s==Side::White?Side::Black:Side::White;
    }

    // square/move to algebraic like "e2e4"
    inline QString sqStr(uint8_t s){
        static const char* files = "abcdefgh";
        return QString("%1%2").arg(files[s & 7]).arg((s >> 3) + 1);
    }

    static QString MoveStr(const Move& m){
        auto sq = [](uint8_t s){ const char* f="abcdefgh"; return QString("%1%2").arg(f[s&7]).arg((s>>3)+1); };
        return sq(m.GetFrom()) + sq(m.GetTo());
    }

    // quick generators
    inline void GenAll(const Position& pos, Side side, MoveList& out){
        LegalMoveGen::Generate(pos, side, out, /*only_captures=*/false);
    }

    static uint64_t PerftCount(Position& pos, Side stm, int depth) {
        if (depth == 0) return 1;
        MoveList ml; LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/false);
        uint64_t nodes = 0;
        for (uint32_t i = 0; i < ml.GetSize(); ++i) {
            Position::Undo u{}; pos.ApplyMove(ml[i], u);
            nodes += PerftCount(pos, (stm==Side::White?Side::Black:Side::White), depth - 1);
            pos.UndoMove(ml[i], u);
        }
        return nodes;
    }

    // tiny perft helpers
    static uint64_t Perft1(Position& pos, Side side){
        MoveList list; GenAll(pos, side, list);
        return list.GetSize();
    }
    static uint64_t Perft2(Position& pos, Side side){
        MoveList list; GenAll(pos, side, list);
        uint64_t n = 0;
        for (uint32_t i=0;i<list.GetSize();++i){
            Position::Undo u{}; pos.ApplyMove(list[i], u);
            n += Perft1(pos, Opp(side));
            pos.UndoMove(list[i], u);
        }
        return n;
    }

    // shallow state compare (без тяжёлых проверок)
    static bool ShallowEqual(const Position& a, const Position& b){
        if (a.GetZobristKey()        != b.GetZobristKey())        return false;
        if (a.GetEnPassantSquare()   != b.GetEnPassantSquare())   return false;
        if (a.GetWhiteLongCastling() != b.GetWhiteLongCastling()) return false;
        if (a.GetWhiteShortCastling()!= b.GetWhiteShortCastling())return false;
        if (a.GetBlackLongCastling() != b.GetBlackLongCastling()) return false;
        if (a.GetBlackShortCastling()!= b.GetBlackShortCastling())return false;
        return true;
    }
} // namespace

// ---------------- Tests ----------------

void LegalMoveGenTest::ApplyUndoRoundTrip() {
    // стартовая позиция
    Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                 Position::NONE, true,true,true,true, /*moveCounter=*/0);
    Position start = pos;

    MoveList list; GenAll(pos, Side::White, list);
    QVERIFY(list.GetSize() > 0);

    for (uint32_t i=0; i<list.GetSize(); ++i) {
        Position::Undo u{};
        pos.ApplyMove(list[i], u);

        // свой король не под боем
        uint8_t ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(Side::White, PieceType::King));
        QVERIFY(!PsLegalMaskGen::SquareInDanger(pos.GetPieces(), ksq, Side::White));

        pos.UndoMove(list[i], u);
        QVERIFY(ShallowEqual(pos, start));
    }
}

void LegalMoveGenTest::EnPassantShouldAppearWhenAvailable() {
    // EP = e6 (41), ход белых — из набора EP-позиций
    const uint8_t ep = 41;
    Position pos("rnbqkbnr/p1p1pppp/8/1pPp4/8/8/PP1PPPPP/RNBQKBNR",
                 ep, true,true,true,true, /*moveCounter=*/0);
    MoveList list; GenAll(pos, Side::White, list);

    bool hasEP = false;
    for (uint32_t i=0;i<list.GetSize();++i)
        if (list[i].GetFlag()==Move::Flag::EnPassantCapture){ hasEP=true; break; }
    QVERIFY(hasEP);
}

void LegalMoveGenTest::CastlingMovesShouldAppearWhenLegal() {
    // kiwipete — у белых обе рокировки доступны
    Position pos("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R",
                 Position::NONE, true,true,true,true, /*moveCounter=*/0);
    MoveList list; GenAll(pos, Side::White, list);

    bool hasOO=false, hasOOO=false;
    for (uint32_t i=0;i<list.GetSize();++i){
        auto f = list[i].GetFlag();
        if (f==Move::Flag::WhiteShortCastling) hasOO = true;
        if (f==Move::Flag::WhiteLongCastling ) hasOOO = true;
    }
    QVERIFY(hasOO);
    QVERIFY(hasOOO);
}

void LegalMoveGenTest::PromotionsShouldComeInPacksOfFour() {
    // простая сцена для промо
    Position pos("8/P7/8/8/8/8/7p/7K",
                 Position::NONE, false,false,false,false, /*moveCounter=*/0);

    MoveList list; GenAll(pos, Side::White, list);
    int promos = 0;
    for (uint32_t i=0;i<list.GetSize();++i){
        auto f=list[i].GetFlag();
        if (f==Move::Flag::PromoteToKnight || f==Move::Flag::PromoteToBishop ||
            f==Move::Flag::PromoteToRook   || f==Move::Flag::PromoteToQueen) ++promos;
    }
    QVERIFY(promos % 4 == 0);
}

void LegalMoveGenTest::Perft_StartPos() {
    Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                 Position::NONE, true,true,true,true, 0);
    QCOMPARE(Perft1(pos, Side::White), 20ull);
    QCOMPARE(Perft2(pos, Side::White), 400ull);
}

void LegalMoveGenTest::Perft_Kiwipete() {
    Position pos("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R",
                 Position::NONE, true,true,true,true, 0);
    QCOMPARE(Perft1(pos, Side::White), 48ull);
    QCOMPARE(Perft2(pos, Side::White), 2039ull);
}

void LegalMoveGenTest::Perft_EnPassant() {
    Position pos("rnbqkbnr/p1p1pppp/8/1pPp4/8/8/PP1PPPPP/RNBQKBNR",
                 41, true,true,true,true, 0);
    QCOMPARE(Perft1(pos, Side::White), 23ull);
    QCOMPARE(Perft2(pos, Side::White), 643ull);
}

void LegalMoveGenTest::Perft_Endgame() {
    Position pos("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8",
                 Position::NONE, false,false,false,false, 0);
    QCOMPARE(Perft1(pos, Side::White), 14ull);
    QCOMPARE(Perft2(pos, Side::White), 191ull);
}

// // Временный отладочный тест
// void LegalMoveGenTest::Perft_StartPos_Divide4_Print() {
//     Position pos("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R",
//                  Position::NONE, true,true,false,false, 0);
//     MoveList ml;
//     LegalMoveGen::Generate(pos, Side::White, ml, /*only_captures=*/false);

//     // чтобы сравнивать аккуратно — отсортируем по строке хода
//     struct Item { QString m; uint64_t n; };
//     QVector<Item> items;
//     items.reserve(ml.GetSize());

//     for (uint32_t i=0;i<ml.GetSize();++i){
//         Position::Undo u{};
//         pos.ApplyMove(ml[i], u);
//         uint64_t n = PerftCount(pos, Side::Black, 3); // итого depth=4
//         pos.UndoMove(ml[i], u);
//         items.push_back({MoveStr(ml[i]), n});
//     }
//     std::sort(items.begin(), items.end(), [](const Item&a,const Item&b){ return a.m<b.m; });
//     uint64_t total=0;
//     for (auto& it: items){
//         qDebug() << it.m << ":" << it.n;
//         total += it.n;
//     }
//     qDebug() << "TOTAL:" << total;
//     // Сверяем общую сумму тоже, чтобы не ошибиться руками
//     QCOMPARE(total, 2103487ull);
// }

static uint64_t PerftRec(Position& pos, Side stm, int depth){
    if (depth==0) return 1;
    MoveList ml; LegalMoveGen::Generate(pos, stm, ml, false);
    uint64_t n=0;
    for (uint32_t i=0;i<ml.GetSize();++i){
        Position::Undo u{}; pos.ApplyMove(ml[i], u);
        n += PerftRec(pos, (stm==Side::White?Side::Black:Side::White), depth-1);
        pos.UndoMove(ml[i], u);
    }
    return n;
}

static QString mv(const Move& m){
    auto sq=[&](uint8_t s){ const char* f="abcdefgh"; return QString("%1%2").arg(f[s&7]).arg((s>>3)+1); };
    return sq(m.GetFrom()) + sq(m.GetTo());
}

static QString moveWithPromo(const Move& m)
{
    auto sq = [](uint8_t s)
    {
        const char* f = "abcdefgh";
        return QString("%1%2").arg(f[s & 7]).arg((s >> 3) + 1);
    };

    QString s = sq(m.GetFrom()) + sq(m.GetTo());

    switch (m.GetFlag()) {
    case Move::Flag::PromoteToQueen:  s += "=q"; break;
    case Move::Flag::PromoteToRook:   s += "=r"; break;
    case Move::Flag::PromoteToBishop: s += "=b"; break;
    case Move::Flag::PromoteToKnight: s += "=n"; break;
    default: break;
    }
    return s;
}

static void debugBlackRepliesAfter(const Position& root, const Move& m)
{
    Position pos = root;
    Position::Undo u{};
    pos.ApplyMove(m, u);

    MoveList bl;
    LegalMoveGen::Generate(pos, Side::Black, bl, false);

    qDebug() << "BLACK replies after" << moveWithPromo(m) << ", count =" << bl.GetSize();
    for (uint32_t i = 0; i < bl.GetSize(); ++i) {
        qDebug() << " " << moveWithPromo(bl[i]);
    }

    pos.UndoMove(m, u);
}

void LegalMoveGenTest::Debug_Divide_Position2_d4() {
    Position pos("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R",
                 Position::NONE, /*W O-O-O*/true,/*W O-O*/true,/*B O-O-O*/true,/*B O-O*/true, 0);
    MoveList ml;
    LegalMoveGen::Generate(pos, Side::White, ml, false);

    struct Item{ QString m; uint64_t n; };
    QVector<Item> v;
    v.reserve(ml.GetSize());
    for (uint32_t i=0;i<ml.GetSize();++i){
        Position::Undo u{};
        pos.ApplyMove(ml[i], u);
        uint64_t n = PerftRec(pos, Side::Black, 3); // итого depth=4
        pos.UndoMove(ml[i], u);
        v.push_back({mv(ml[i]), n});
    }
    std::sort(v.begin(), v.end(), [](auto&a,auto&b){ return a.m<b.m; });

    // uint64_t total=0;
    // for (auto& it: v){
    //     qDebug() << it.m << ":" << it.n;
    //     total += it.n;
    // }
    // qDebug() << "TOTAL:" << total; // ожидаем 2'103'487
}

