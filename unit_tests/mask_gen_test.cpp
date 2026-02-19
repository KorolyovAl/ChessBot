#include "mask_gen_test.h"

#include "../ChessBot/src/engine_core/board_state/pieces.h"
#include "../ChessBot/src/engine_core/board_state/position.h"
#include "../ChessBot/src/engine_core/move_generation/pawn_attack_masks.h"
#include "../ChessBot/src/engine_core/move_generation/ps_legal_move_mask_gen.h"

/*──────────────────── Pawn attack LUT ───────────────────*/

/* 1. Checks that white pawn on e4 attacks d5 & f5, nothing else */
void MaskGenTest::PawnAttackMaskShouldMatchExpected() {
    int e4 = 28;              // square index (4,3)
    Bitboard mask = PawnMasks::kAttack[static_cast<int>(Side::White)][e4];

    QVERIFY(BOp::GetBit(mask, 37));   // f5  (5,4)
    QVERIFY(BOp::GetBit(mask, 35));   // d5  (3,4)

    /* The pawn should NOT attack off‑board squares */
    QVERIFY(!BOp::GetBit(mask, 45));   // g6 out of reach
    QVERIFY(!BOp::GetBit(mask, 27));   // e4 itself
}

/*──────────────────── Rook ray cutting ──────────────────*/

/* 2. a1 rook, own knight on a3 ➜ mask ends at a2, a3 excluded */
void MaskGenTest::RookRayShouldStopBeforeOwnBlocker() {
    // FEN: white rook a1 (0), white knight a3 (16)
    Position pos("8/8/8/8/8/N7/8/R7", Position::NONE,
                 false, false, false, false, 0);

    Bitboard ray = PsLegalMaskGen::RookMask(pos.GetPieces(), 0, Side::White, false);

    QVERIFY(BOp::GetBit(ray, 8));   // a2 included
    QVERIFY(!BOp::GetBit(ray, 16));   // a3 (own piece) excluded
    QVERIFY(!BOp::GetBit(ray, 24));   // a4 trimmed
}

/*──────────────────── Bishop capture ray ───────────────*/

/* 3. bishop c1, enemy pawn e3 ➜ e3 capturable and ray stops */
void MaskGenTest::BishopRayShouldIncludeCaptureSquare() {
    Pieces pcs("8/8/8/8/8/4p3/8/2B5");   // bishop c1 (2), pawn e3 (20)
    Bitboard mask = PsLegalMaskGen::BishopMask(pcs, 2, Side::White, false);

    QVERIFY(BOp::GetBit(mask, 20));    // e3 capture square included
    QVERIFY(!BOp::GetBit(mask, 38));    // f4 beyond pawn excluded
}

/*──────────────────── Danger detection ─────────────────*/

/* 4. black pawn e5 checks white king f4 */
void MaskGenTest::SquareInDangerShouldDetectPawnCheck() {
    Pieces pcs("8/8/8/4p3/5K2/8/8/8");   // king f4 (29), pawn e5 (36)

    for (int sq = 0; sq < 64; ++sq)
        if (BOp::GetBit(pcs.GetAllBitboard(), sq))
            qDebug() << "piece on" << sq;

    // qDebug() << "Danger mask:";
    // auto danger_mask = PsLegalMaskGen::PawnCaptureMask(pcs, Side::Black, true);
    // for (int r = 7; r >= 0; --r) {
    //     QString line;
    //     for (int f = 0; f < 8; ++f) {
    //         int sq = r * 8 + f;
    //         line += BOp::GetBit(danger_mask, sq) ? '1' : '.';
    //     }
    //     qDebug().noquote() << line;
    // }

    QVERIFY(PsLegalMaskGen::SquareInDanger(pcs, 29, Side::White));
}

/* 5. rook capture square included when enemy blocks */
void MaskGenTest::RookRayShouldIncludeCaptureSquare() {
    Pieces pcs("8/8/8/8/r7/8/8/R7");   // white rook a1 (0), black rook a4 (24)
    Bitboard ray = PsLegalMaskGen::RookMask(pcs, 0, Side::White, false);

    QVERIFY(BOp::GetBit(ray, 24));    // capture square
    QVERIFY(!BOp::GetBit(ray, 32));    // beyond blocker excluded
}

/* 6. knight_mask only_captures flag */
void MaskGenTest::KnightMaskOnlyCapturesShouldFilter() {
    Pieces pcs("8/8/8/8/8/3r4/8/2N5");  // white knight c1 (2), enemy rook d3 (19)
    Bitboard all = PsLegalMaskGen::KnightMask(pcs, 2, Side::White, false);
    Bitboard cap = PsLegalMaskGen::KnightMask(pcs, 2, Side::White, true);

    QVERIFY(BOp::GetBit(all, 19));          // move present normally
    QVERIFY(BOp::GetBit(cap, 19));          // capture present
    QVERIFY(all != cap);                    // but cap‑mask is subset
}

/* 7. pawn_double_push blocked by piece */
void MaskGenTest::PawnDoublePushShouldRespectBlockers() {
    Pieces pcs("8/8/8/8/8/p7/P7/8");  // black pawn e3 blocks a2 double push
    Bitboard dbl = PsLegalMaskGen::PawnDoublePush(pcs, Side::White);

    QVERIFY(dbl == 0);               // no double because path blocked
}

/* 8. pawn_capture_mask include_all_attacks flag */
void MaskGenTest::PawnCaptureMaskAllAttacksVsLegal() {
    Pieces pcs("8/8/8/8/8/8/2p1P3/8"); // white pawn e2 (12), black pawn c2 (10)
    Bitboard all = PsLegalMaskGen::PawnCaptureMask(pcs, Side::White, true);
    Bitboard legal = PsLegalMaskGen::PawnCaptureMask(pcs, Side::White, false);

    QVERIFY(BOp::GetBit(all, 21));       // d3 square in attack map
    QVERIFY(!BOp::GetBit(legal, 21));     // but not legal (empty)
}

/* 9. bishop_mask on empty board equals pre‑computed rays */
void MaskGenTest::BishopMaskOnEmptyBoardEqualsTable() {
    Pieces pcs("8/8/8/8/8/8/8/2B5");   // bishop c1 (2)
    Bitboard mask = PsLegalMaskGen::BishopMask(pcs, 2, Side::White, false);

    using D = SlidersMasks::Direction;
    Bitboard expected = SlidersMasks::kMasks[2][D::NorthEast] |
                        SlidersMasks::kMasks[2][D::NorthWest];

    QCOMPARE(mask, expected);
}

/* 10. square_in_danger false when no attacks */
void MaskGenTest::SquareInDangerShouldReturnFalseWhenSafe() {
    Pieces pcs("8/8/8/8/8/8/8/2K5");   // lone king c1 (2)
    //Pieces pcs("rnb1kbnr/pppp1ppp/4pq2/8/8/1P2P3/P1PP1PPP/RNBQKBNR");
    QVERIFY(!PsLegalMaskGen::SquareInDanger(pcs, 2, Side::White));
}
