#include "see_test.h"

#include "../ChessBot/board_state/pieces.h"
#include "../ChessBot/board_state/move.h"
#include "../ChessBot/ai_logic/static_exchange_evaluation.h"

void SeeTest::LosingCapture_ShouldBeNegative() {
    // White knight captures a defended pawn: Nxd5 loses to cxd5.
    Pieces pcs("8/8/8/3p4/8/8/8/1N6");
    Move m(1, 27, static_cast<uint8_t>(PieceType::Knight),
           static_cast<uint8_t>(Side::White),
           static_cast<uint8_t>(PieceType::Pawn),
           static_cast<uint8_t>(Side::Black),
           Move::Flag::Capture);

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see < 0, "Losing capture must have negative SEE");
}

void SeeTest::WinningCapture_ShouldBePositive() {
    // White bishop takes an undefended rook.
    Pieces pcs("7r/8/8/8/8/8/8/3B4");
    Move m(27, 7, static_cast<uint8_t>(PieceType::Bishop),
           static_cast<uint8_t>(Side::White),
           static_cast<uint8_t>(PieceType::Rook),
           static_cast<uint8_t>(Side::Black),
           Move::Flag::Capture);

    const int see = StaticExchangeEvaluation::Capture(pcs, m);
    QVERIFY2(see > 0, "Winning capture must have positive SEE");
}
