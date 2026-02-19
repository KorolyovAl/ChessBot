#include "evaluation_test.h"

#include "../ChessBot/src/engine_core/board_state/position.h"
#include "../ChessBot/src/engine_core/ai_logic/evaluation.h"

namespace {
    // Helper: create startpos with given side to move.
    // Castling rights set to true to mirror a normal start.
    static Position StartPos(bool whiteToMove) {
        return Position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                        Position::NONE, true, true, true, true,
                        /*moveCounter=*/whiteToMove ? 0 : 1);
    }
} // namespace

void EvaluationTest::Tempo_ShouldBeAbout10() {
    // White to move: symmetric position → expect ~+10 (tempo only).
    {
        Position pos = StartPos(/*whiteToMove=*/true);
        const int s = Evaluation::Evaluate(pos);
        qDebug() << "Evaluate: " << s;
        QVERIFY2(s >= 8 && s <= 12, "Tempo for White should be around +10 cp");
    }
    // Black to move: mirror sign.
    {
        Position pos = StartPos(/*whiteToMove=*/false);
        const int s = Evaluation::Evaluate(pos);
        qDebug() << "Evaluate: " << s;
        QVERIFY2(s >= -12 && s <= -8, "Tempo for Black should be around -10 cp");
    }
}

void EvaluationTest::PST_KnightCenterBeatsCorner() {
    // Central knight on d4 (kings on e1/e8).
    Position center("4k3/8/8/8/3N4/8/8/4K3",
                    Position::NONE, false, false, false, false, 0);
    // Corner knight on a1 (everything else same).
    Position corner("4k3/8/8/8/8/8/8/N3K3",
                    Position::NONE, false, false, false, false, 0);

    const int s_center = Evaluation::Evaluate(center);
    const int s_corner = Evaluation::Evaluate(corner);

    qDebug() << "Evaluate sc: " << s_center;
    qDebug() << "Evaluate sk: " << s_corner;

    // PST (and mobility) should strongly prefer the center.
    QVERIFY2(s_center - s_corner >= 40, "A centralized knight must evaluate at least ~40 cp better than a corner knight");
}

void EvaluationTest::BishopPair_ShouldBePositive() {
    // White: two bishops vs Black: one bishop; kings on e1/e8.
    // Material + bishop pair bonus should clearly dominate (>+300cp).
    Position pos("2b1k3/8/8/8/8/8/8/2B1KB2",
                 Position::NONE, false, false, false, false, 0);
    const int s = Evaluation::Evaluate(pos);
    QVERIFY2(s >= 300, "Bishop pair should yield a clearly positive swing for White (>= +300 cp)");
}

void EvaluationTest::Material_ShouldReflectPawnImbalance() {
    // A) Kings only.
    Position base("4k3/8/8/8/8/8/8/4K3",
                  Position::NONE, false, false, false, false, 0);

    // B) Same but White has an extra pawn on e4.
    Position plusPawn("4k3/8/8/8/4P3/8/8/4K3",
                      Position::NONE, false, false, false, false, 0);

    const int sA = Evaluation::Evaluate(base);
    const int sB = Evaluation::Evaluate(plusPawn);

    // Expect roughly +100 cp (allow tolerance for PST/phase/tempo).
    QVERIFY2(sB - sA >= 90, "One extra pawn should be worth ~100 cp");
}

void EvaluationTest::PassedPawn_BonusStrongerInEndgame() {
    // Same passed pawn but with different game phases:
    // - MG-like: both sides keep queen + two rooks (phase ≈ 16/24).
    // - EG-like: only kings + the same passer (phase = 0/24).
    //
    // We place White's passed pawn on e6 (rank 6), where our tables give:
    //   MG bonus = 60 cp, EG bonus = 80 cp.
    // With phase ≈ 16 in MG, the tapered contribution becomes:
    //   sMG ≈ (60*16 + 80*8) / 24 = 66.6… cp
    //   sEG  = 80 cp
    // Delta ≈ 13.3 cp → use a safe threshold of 12 cp.

    // Middlegame-ish: QRR vs QRR + White passer on e6.
    // Ranks (top → bottom): 8/7/6/5/4/3/2/1
    // Third rank from the top is the 6th rank → "4P3"
    Position mg("r2qk2r/8/4P3/8/8/8/8/R2QK2R",
                Position::NONE, false, false, false, false, 0);

    // Endgame-ish: only kings + the same passer on e6.
    Position eg("4k3/8/4P3/8/8/8/8/4K3",
                Position::NONE, false, false, false, false, 0);

    const int sMG = Evaluation::Evaluate(mg);
    const int sEG = Evaluation::Evaluate(eg);

    QVERIFY2(sEG - sMG >= 12,
             "A passed pawn should evaluate higher in EG than in MG (tapered bonus with e6 passer).");
}

void EvaluationTest::KingSafety_ShieldAndOpenColumnsMatter() {
    // Compare two positions with White castled short:
    // A) Full pawn shield f2/g2/h2 and no open columns near the king.
    // B) g2 pawn removed (open/half-open column near king) + same attacking queen on g4.
    //
    // Expect B to score lower for White.

    // A: solid shield
    Position safe("6k1/8/8/8/6q1/8/5PPP/6K1",
                  Position::NONE, false, false, false, false, 0);

    // B: missing g2 pawn → weaker shield / open column near king
    Position exposed("6k1/8/8/8/6q1/8/5P1P/6K1",
                     Position::NONE, false, false, false, false, 0);

    const int sSafe = Evaluation::Evaluate(safe);
    const int sExposed = Evaluation::Evaluate(exposed);

    QVERIFY2(sExposed < sSafe, "Missing pawn shield / open column near the king should worsen evaluation");
}

void EvaluationTest::Benchmark_Evaluate_MidgameDense() {
    // Dense middlegame scene (commonly used in engine benches).
    // We keep construction outside the measurement loop.
    Position pos("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1",
                 Position::NONE, false, false, false, false, 0);

    // QBENCHMARK automatically runs Evaluate() many times and reports
    // average time per iteration (microseconds) in the Test Results panel.
    QBENCHMARK {
        volatile int sink = Evaluation::Evaluate(pos);
        (void)sink; // prevent elision
    }
}

void EvaluationTest::Test_LaskerTrap() {
    Position pos("r2q1rk1/ppp2ppp/2n2n2/3pp3/3P4/2PBPN2/PP3PPP/R1BQ1RK1",
                 Position::NONE, false, false, false, false, 0);

    test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

    QVERIFY2(eval.common >= 250,
             "At Lasker Trap the white has more 2 pawn eval points without NNUE");
}

void EvaluationTest::Test_BratkoKopec() {
    Position pos("r3k2r/pp1b1ppp/2p1pn2/q7/3P4/2N1BN2/PP3PPP/R2Q1RK1",
                 Position::NONE, false, false, true, true, 0);

    test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

    QVERIFY2(eval.common >= 250,
             "At Bratko-Kopec the white has more 2 pawn eval points without NNUE");

}

void EvaluationTest::Test_From_Game_1() {
    {
        Position pos("rnb1kbnr/pppp1ppp/4pq2/8/8/1P2P3/P1PP1PPP/RNBQKBNR",
                     Position::NONE, true, true, true, true, 0);

        test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

        QVERIFY2(eval.common <= 50,
                 "At this position evaluation must be less 50 cp");
    }

    {
        Position pos("rnb1kbnr/pppp1ppp/4p3/8/8/1PN1P3/P1PP1qPP/R1BQKBNR",
                     Position::NONE, true, true, true, true, 0);

        test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

        QVERIFY2(eval.common >= 700,
                 "At this position evaluation must be more 700 cp");
    }

    {
        Position pos("rnb1kbnr/pp1ppppp/1qp5/8/8/1P2P3/PBPP1PPP/RN1QKBNR",
                     Position::NONE, true, true, true, true, 1);

        test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

        QVERIFY2(eval.common <= 100,
                 "At this position evaluation must be less 100 cp");
    }

    {
        Position pos("rnb1kbnr/pp1ppppp/2p5/8/8/1q2P3/PBPP1PPP/RN1QKBNR",
                     Position::NONE, true, true, true, true, 0);

        test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);

        QVERIFY2(eval.common >= 400,
                 "At this position the white side has a advantage");
    }

    {
        Position pos("rnb1k1r1/2pp1ppp/1p2p2n/p7/2PPP3/P1N2B2/1P3KPP/R1BQ3R",
                     Position::NONE, false, false, true, false, 2);

        test::EvaluatePos eval = Evaluation::EvaluateForTest(pos);
    }
}
