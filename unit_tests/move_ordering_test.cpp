#include "move_ordering_test.h"

#include "../ChessBot/src/engine_core/board_state/pieces.h"
#include "../ChessBot/src/engine_core/board_state/move.h"
#include "../ChessBot/src/engine_core/ai_logic/move_ordering.h"

namespace {
Pieces SimpleMaterial() {
    // White: knight b1; Black: pawn c3. Others empty.
    // This is just to give MVV-LVA some context.
    return Pieces("8/8/2p5/8/8/8/8/1N6");
}
} // namespace

void MoveOrderingTest::TtMove_ShouldOutscoreOthers() {
    Pieces pcs = SimpleMaterial();

    Move tt(1, 18, static_cast<uint8_t>(PieceType::Knight),
            static_cast<uint8_t>(Side::White),
            Move::None, static_cast<uint8_t>(Side::Black),
            Move::Flag::Default);

    Move q(1, 9, static_cast<uint8_t>(PieceType::Knight),
           static_cast<uint8_t>(Side::White),
           Move::None, static_cast<uint8_t>(Side::Black),
           Move::Flag::Default);

    static int hist[2][64][64]{};
    MoveOrdering::Context ctx{};
    ctx.tt_move = tt;
    ctx.cutoff1 = 0;
    ctx.cutoff2 = 0;
    ctx.history = &hist;
    ctx.side_to_move = Side::White;

    const int s_tt = MoveOrdering::Score(tt, pcs, ctx);
    const int s_q  = MoveOrdering::Score(q,  pcs, ctx);

    QVERIFY2(s_tt > s_q, "TT move must have the highest score");
}

void MoveOrderingTest::Capture_ShouldOutscoreQuiet() {
    Pieces pcs = SimpleMaterial();

    Move cap(1, 18, static_cast<uint8_t>(PieceType::Knight),
             static_cast<uint8_t>(Side::White),
             static_cast<uint8_t>(PieceType::Pawn),
             static_cast<uint8_t>(Side::Black),
             Move::Flag::Capture);

    Move quiet(1, 9, static_cast<uint8_t>(PieceType::Knight),
               static_cast<uint8_t>(Side::White),
               Move::None, static_cast<uint8_t>(Side::Black),
               Move::Flag::Default);

    MoveOrdering::Context ctx{};
    ctx.tt_move = Move{};
    ctx.cutoff1 = 0;
    ctx.cutoff2 = 0;
    ctx.history = nullptr;
    ctx.side_to_move = Side::White;

    const int s_cap = MoveOrdering::Score(cap, pcs, ctx);
    const int s_quiet = MoveOrdering::Score(quiet, pcs, ctx);

    QVERIFY2(s_cap > s_quiet, "Capture should score higher than a simple move");
}
