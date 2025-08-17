#include "see_test.h"

#include "../ChessBot/board_state/pieces.h"
#include "../ChessBot/board_state/move.h"
#include "../ChessBot/ai_logic/static_exchange_evaluation.h"

void SeeTest::LosingCapture_ShouldBeNegative() {
    Pieces pcs("r7/p6/8/8/8/8/8/Q7");

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
