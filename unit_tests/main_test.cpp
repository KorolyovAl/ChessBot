#include <QTest>
#include "pieces_test.h"
#include "bitboard_test.h"
#include "move_test.h"
#include "zobrist_hash_test.h"
#include "position_test.h"
#include "mask_gen_test.h"
#include "legal_move_gen_test.h"

#include "legal_move_gen_tester.h"

int main(int argc, char** argv) {
    int status = 0;

    {
        //LegalMoveGenTester::RunTests();
    }

    {
        LegalMoveGenTest lmg;
        status |= QTest::qExec(&lmg, argc, argv);
    }

    {
        PiecesTest pieces;
        status |= QTest::qExec(&pieces, argc, argv);
    }

    {
        BitboardTest bitboard;
        status |= QTest::qExec(&bitboard, argc, argv);
    }

    {
        MoveTest move;
        status |= QTest::qExec(&move, argc, argv);
    }

    {
        ZobristHashTest zobrist;
        status |= QTest::qExec(&zobrist, argc, argv);
    }

    {
        PositionTest position_test;
        status |= QTest::qExec(&position_test, argc, argv);
    }

    {
        MaskGenTest masks_test;
        status |= QTest::qExec(&masks_test, argc, argv);
    }

    return status;
}
