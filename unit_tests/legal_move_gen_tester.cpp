#include "legal_move_gen_tester.h"

#include "../ChessBot/src/engine_core/move_generation/legal_move_gen.h"
#include "../ChessBot/src/engine_core/move_generation/move_list.h"

static inline Side Opposite(Side s) {
    return s == Side::White ? Side::Black : Side::White;
}

void LegalMoveGenTester::RunTests() {
    std::array<Test, 7> tests{};

    tests[0] = Test{
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        Position::NONE, true, true, true, true, Side::White,
        { 1ULL, 20ULL, 400ULL, 8'902ULL, 197'281ULL, 4'865'609ULL }
    };
    tests[1] = Test{
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R",
        Position::NONE, true, true, false, false, Side::White,
        { 1ULL, 44ULL, 1'486ULL, 62'379ULL, 2'103'487ULL, 89'941'194ULL }
    };
    tests[2] = Test{
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8",
        Position::NONE, false, false, false, false, Side::White,
        { 1ULL, 14ULL, 191ULL, 2'812ULL, 43'238ULL, 674'624ULL }
    };
    tests[3] = Test{
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1",
        Position::NONE, false, false, true, true, Side::White,
        { 1ULL, 6ULL, 264ULL, 9'467ULL, 422'333ULL, 15'833'292ULL }
    };
    tests[4] = Test{
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R",
        Position::NONE, true, true, true, true, Side::White,
        { 1ULL, 48ULL, 2'039ULL, 97'862ULL, 4'085'603ULL, 193'690'690ULL }
    };
    tests[5] = Test{
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1",
        Position::NONE, false, false, false, false, Side::White,
        { 1ULL, 46ULL, 2'079ULL, 89'890ULL, 3'894'594ULL, 164'075'551ULL }
    };
    tests[6] = Test{
        "bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR", // позиция для шахмат Фишера, рокировки считаются не правильно
        Position::NONE, false, false, false, false, Side::White,
        { 1ULL, 21ULL, 528LL, 12'189ULL, 326'672ULL, 8'146'062ULL }
    };

    int i = 0;
    for (const auto& t : tests) {
        if (t.nodes[0] == 0) continue; // пустые слоты
        std::cout << "Test #" << i << std::endl;
        RunTest(t);
        ++i;
    }
}

void LegalMoveGenTester::RunTest(const Test& test) {
    // Конструктор Position: последний аргумент — move_counter (чётный=белые, нечётный=чёрные)
    float mc = (test.side == Side::White) ? 0.f : 1.f;
    Position pos(test.shortFen, test.enPassant,
                 test.wlCastling, test.wsCastling,
                 test.blCastling, test.bsCastling,
                 mc); // см. сигнатуру конструктора

    for (uint32_t depth = 0; depth < test.nodes.size(); ++depth) {
        uint64_t start = nsecs;
        uint64_t got = Perft(pos, test.side, depth);
        uint64_t correct = test.nodes[depth];

        float secs = float(nsecs - start) / 1e9f;
        float mnps = secs > 0.f ? (float)got / secs / 1e6f : 0.f;

        std::cout << "Depth " << std::setw(4) << depth
                  << ". Correct: " << std::setw(18) << correct
                  << ". Got: " << std::setw(18) << got
                  << ". Speed: " << std::setw(10) << mnps << " MNPS. "
                  << (got == correct ? "OK." : "Error.") << std::endl;
    }
    std::cout << std::endl;
}

uint64_t LegalMoveGenTester::Perft(Position& position, Side side, uint32_t depth) {
    if (depth == 0) return 1ULL;

    MoveList moves;
    LegalMoveGen::Generate(position, side, moves, /*only_captures=*/false); //

    uint64_t nodes = 0;
    for (uint32_t i = 0; i < moves.GetSize(); ++i) { // если у тебя GetSize(), просто замени
        const Move& m = moves[i];

        Position::Undo u{};
        position.ApplyMove(m, u);              // применять на месте
        nodes += Perft(position, Opposite(side), depth - 1);
        position.UndoMove(m, u);
    }
    return nodes;
}
