#include "see_test.h"

#include "../ChessBot/src/engine_core/board_state/pieces.h"
#include "../ChessBot/src/engine_core/board_state/move.h"
#include "../ChessBot/src/engine_core/ai_logic/static_exchange_evaluation.h"

void SeeTest::LosingCapture_ShouldBeNegative() {
    Pieces pcs("r7/p7/8/8/8/8/8/Q7");

    // Ход: Qa1xa7
    Move m(
        /*from*/ 0,   /* a1 */
        /*to*/   48,  /* a7 */
        static_cast<uint8_t>(PieceType::Queen),  static_cast<uint8_t>(Side::White),
        static_cast<uint8_t>(PieceType::Pawn),   static_cast<uint8_t>(Side::Black),
        Move::Flag::Capture
        );

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see < 0, "Losing capture must have negative SEE");
}

void SeeTest::WinningCapture_ShouldBePositive() {
    // На доске: чёрная ладья c3; белый слон a1. Больше никого.
    Pieces pcs("8/8/8/8/8/2r5/8/B7");

    // Ход: Ba1xc3
    Move m(/*from*/ 0,  /*to*/ 18,
           static_cast<uint8_t>(PieceType::Bishop), static_cast<uint8_t>(Side::White),
           static_cast<uint8_t>(PieceType::Rook),   static_cast<uint8_t>(Side::Black),
           Move::Flag::Capture);

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see > 0, "Winning capture must have positive SEE");
}

void SeeTest::On_QxBf1_ShouldBeNegative() {
    // rnb1kbnr/pppp1ppp/4p3/8/8/1PN1P3/P1PP1qPP/R1BQKBNR
    // На f1 белый слон, на f2 чёрная ферзь; Qxf1 проигрышно из-за Kxf2.
    Pieces pcs("rnb1kbnr/pppp1ppp/4p3/8/8/1PN1P3/P1PP1qPP/R1BQKBNR");

    const uint8_t f1 = 5;
    const int see = StaticExchangeEvaluation::On(pcs, f1, Side::White);

    QVERIFY2(see < 0, "SEE.On(f1) must be negative (Qxf1 loses to Kxf2).");
}

void SeeTest::Capture_QxBf1_ShouldBeNegative() {
    // Та же позиция: чёрная ферзь с f2 бьёт слона f1.
    Pieces pcs("rnb1kbnr/pppp1ppp/4p3/8/8/1PN1P3/P1PP1qPP/R1BQKBNR");

    const uint8_t f2 = 13;
    const uint8_t f1 = 5;

    Move m(/*from*/ f2, /*to*/ f1,
           static_cast<uint8_t>(PieceType::Queen), static_cast<uint8_t>(Side::Black),
           static_cast<uint8_t>(PieceType::Bishop), static_cast<uint8_t>(Side::White),
           Move::Flag::Capture);

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see < 0, "SEE.Capture(Qxf1) must be negative.");
}

void SeeTest::On_IllegalKingRecapture_Ignored() {
    // Белые: король на е2, слон на d3, ладья на f1
    // Чёрные: король на е8, ладьи на f6 и f8
    // Ладья чёрных на f6 атакует ладью белых на f1 и король не может её забрать,
    // т.к. вторая ладья чёрных защищает первую
    Pieces pcs("4kr2/8/5r2/8/8/3B4/4K3/5R2");

    const uint8_t f1 = 5;
    const int see = StaticExchangeEvaluation::On(pcs, f1, Side::White);

    QVERIFY2(see > 400, "Illegal king recapture must not improve SEE.On, Rf6xf1 is good for black");
}

void SeeTest::Capture_EnPassant_BasicPositive() {
    // Белая пешка e3, чёрная пешка d3. Ход: d3xe2 e.p.
    Pieces pcs("4k3/8/8/8/3K4/3pP3/8/8");

    const uint8_t d3 = 19;
    const uint8_t e2 = 12;

    Move m(/*from*/ d3, /*to*/ e2,
           static_cast<uint8_t>(PieceType::Pawn),  static_cast<uint8_t>(Side::Black),
           static_cast<uint8_t>(PieceType::Pawn),  static_cast<uint8_t>(Side::White),
           Move::Flag::EnPassantCapture);

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see > 0, "SEE.Capture(en-passant) should be positive in a bare position.");
}
