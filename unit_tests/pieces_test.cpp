#include "pieces_test.h"
#include "../ChessBot/board_state/pieces.h"
#include "../ChessBot/utils/bitboard_debug.h"

void PiecesTest::EmptyBoardShouldHaveNoPieces() {
    Pieces p("8/8/8/8/8/8/8/8");
    QCOMPARE(p.GetAllBitboard(), static_cast<Bitboard>(0));
}

void PiecesTest::SimpleFenShouldSetPawnsCorrectly() {
    Pieces p("8/8/8/8/8/8/pppppppp/PPPPPPPP");
    Bitboard white_expected = 0x00000000000000FF;
    Bitboard black_expected = 0x000000000000FF00;

    PrintBitboard(white_expected);

    QCOMPARE(p.GetPieceBitboard(Side::White, PieceType::Pawn), white_expected);
    QCOMPARE(p.GetPieceBitboard(Side::Black, PieceType::Pawn), black_expected);
}

void PiecesTest::SideAndAllAndEmptyShouldMatch() {
    Pieces p("8/8/8/8/8/8/pppppppp/PPPPPPPP");

    Bitboard white = p.GetSideBoard(Side::White);
    Bitboard black = p.GetSideBoard(Side::Black);
    Bitboard all   = p.GetAllBitboard();
    Bitboard empty = p.GetEmptyBitboard();

    QCOMPARE(white, 0x00000000000000FF);
    QCOMPARE(black, 0x000000000000FF00);
    QCOMPARE(all,   white | black);
    QCOMPARE(empty, ~all);
}

void PiecesTest::ManualSetShouldAffectBoards() {
    Pieces p;
    p.SetPieceBitboard(Side::White, PieceType::Knight, 0x0000000000000042);  // b1 + g1

    p.UpdateBitboard();

    QCOMPARE(p.GetPieceBitboard(Side::White, PieceType::Knight), 0x42);
    QCOMPARE(p.GetSideBoard(Side::White), 0x42);
    QCOMPARE(p.GetAllBitboard(), 0x42);
    QCOMPARE(p.GetEmptyBitboard(), ~0x42);
}

void PiecesTest::StartingPositionShouldBeValid() {
    Pieces p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    // White pieces
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::Rook),   0)); // a1
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::Knight), 1)); // b1
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::Bishop), 2)); // c1
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::Queen),  3)); // d1
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::King),   4)); // e1
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::White, PieceType::Pawn),   8)); // a2

    // Black pieces
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::Rook),   56)); // a8
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::Knight), 57)); // b8
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::Bishop), 58)); // c8
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::Queen),  59)); // d8
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::King),   60)); // e8
    QVERIFY(BOp::GetBit(p.GetPieceBitboard(Side::Black, PieceType::Pawn),   48)); // a7
}
