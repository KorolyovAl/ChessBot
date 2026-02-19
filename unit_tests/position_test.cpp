#include "position_test.h"
#include "../ChessBot/src/engine_core/board_state/position.h"

// === Initialization & Basic Mechanics ===

void PositionTest::ShouldInitializeCorrectlyFromFen() {
    // Initializes position from standard FEN with all castling rights
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", Position::NONE,
               true, true, true, true, 0);

    QVERIFY(p.GetWhiteLongCastling());
    QVERIFY(p.GetWhiteShortCastling());
    QVERIFY(p.GetBlackLongCastling());
    QVERIFY(p.GetBlackShortCastling());
    QCOMPARE(p.GetEnPassantSquare(), Position::NONE);
    QVERIFY(p.IsWhiteToMove());
}

void PositionTest::CastlingFlagsShouldToggleCorrectly() {
    // All castling flags set to true, then one manually disabled
    Position p("8/8/8/8/8/8/8/8", Position::NONE, true, true, true, true, 0);
    p.move_counter_ = 0;

    QVERIFY(p.GetWhiteLongCastling());

    // Dummy move (does nothing)
    p.ApplyMove(Move(0, 0, 0, 0, 255, 255, Move::Flag::Default));

    // Disable white long castling manually
    p.DisableCastling(Side::White, true);
    QVERIFY(!p.GetWhiteLongCastling());
}

void PositionTest::ShouldAddAndRemovePieceCorrectly() {
    // Add and remove white rook on a1
    Position p("8/8/8/8/8/8/8/8", Position::NONE, false, false, false, false, 0);

    p.AddPiece(0, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
    Bitboard bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Rook);
    QVERIFY(BOp::GetBit(bb, 0));

    p.RemovePiece(0, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
    bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Rook);
    QVERIFY(!BOp::GetBit(bb, 0));
}

void PositionTest::SideToMoveShouldSwitch() {
    // White pawn on a2 (square 8)
    Position p("8/8/8/8/8/8/P7/8", Position::NONE, false, false, false, false, 0);

    QVERIFY(p.IsWhiteToMove());

    // White pawn moves a2 → a3
    Move m(8, 16,
           static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::Default);
    p.ApplyMove(m);

    QVERIFY(!p.IsWhiteToMove());
}

// === Special Rules ===

void PositionTest::EnPassantShouldSetAndGet() {
    // Set and get en passant square
    Position p;
    p.SetEnPassantSquare(24);
    QCOMPARE(p.GetEnPassantSquare(), static_cast<uint8_t>(24));
}

void PositionTest::PawnLongMoveShouldSetEnPassantCorrectly() {
    // White pawn moves a2 → a4 (double step)
    Position p("8/8/8/8/8/8/P7/8", Position::NONE, false, false, false, false, 0);

    Move m(8, 24, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::PawnLongMove);
    p.ApplyMove(m);

    // En passant target square should be a3 (16)
    QCOMPARE(p.GetEnPassantSquare(), static_cast<uint8_t>(16));
}

void PositionTest::FiftyMoveCounterShouldResetAndIncrement() {
    // Quiet moves increment the counter
    Position p("8/8/8/8/8/8/8/8", Position::NONE, false, false, false, false, 0);

    p.UpdateFiftyMovesCounter(false);
    p.UpdateFiftyMovesCounter(false);
    p.UpdateFiftyMovesCounter(false);
    QCOMPARE(p.fifty_move_counter_, 3);

    // Reset on pawn move or capture
    p.UpdateFiftyMovesCounter(true);
    QCOMPARE(p.fifty_move_counter_, 0);
}

// === En Passant ===

void PositionTest::MoveShouldHandleEnPassantCorrectly() {
    // White pawn on f5 (square 37), black pawn on e5 (36)
    // En passant square set to e6 (44)
    Position p("8/8/8/4pP2/8/8/8/8", 44, false, false, false, false, 0);

    // White pawn captures en passant from f5 to e6
    Move m(37, 44,
           static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::EnPassantCapture);

    p.ApplyMove(m);

    Bitboard white_pawns = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Pawn);
    Bitboard black_pawns = p.GetPieces().GetPieceBitboard(Side::Black, PieceType::Pawn);

    // White pawn now on e6
    QVERIFY(BOp::GetBit(white_pawns, 44));

    // White pawn gone from f5
    QVERIFY(!BOp::GetBit(white_pawns, 37));

    // Black pawn removed from e5
    QVERIFY(!BOp::GetBit(black_pawns, 36));
}

// === Promotions ===

void PositionTest::MoveShouldHandleAllPromotionsCorrectly() {
    // White pawn on e2 (square 12), will promote on e1 (square 4)
    Position p("8/8/8/8/8/8/4P3/8", Position::NONE, false, false, false, false, 0);

    struct {
        Move::Flag flag;
        PieceType expected_piece;
    } promotions[] = {
                      {Move::Flag::PromoteToQueen, PieceType::Queen},
                      {Move::Flag::PromoteToRook, PieceType::Rook},
                      {Move::Flag::PromoteToBishop, PieceType::Bishop},
                      {Move::Flag::PromoteToKnight, PieceType::Knight},
                      };

    for (auto& promo : promotions) {
        Position pos = p;

        // e2 → e1 with promotion
        Move m(12, 4,
               static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
               Move::None, Move::None, promo.flag);

        pos.ApplyMove(m);

        Bitboard promoted = pos.GetPieces().GetPieceBitboard(Side::White, promo.expected_piece);
        Bitboard pawns = pos.GetPieces().GetPieceBitboard(Side::White, PieceType::Pawn);

        // promoted piece on e1
        QVERIFY(BOp::GetBit(promoted, 4));

        // pawn removed from e2
        QVERIFY(!BOp::GetBit(pawns, 12));
    }
}

void PositionTest::MoveShouldHandleCaptureWithPromotionCorrectly() {
    // White pawn on f2 (13), black rook on e1 (4)
    Position p("8/8/8/8/8/8/5P2/4r3", Position::NONE, false, false, false, false, 0);

    // f2 → e1, capturing rook and promoting to queen
    Move m(13, 4,
           static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
           static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black),
           Move::Flag::PromoteToQueen);

    p.ApplyMove(m);

    Bitboard queens = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Queen);
    Bitboard black_rooks = p.GetPieces().GetPieceBitboard(Side::Black, PieceType::Rook);
    Bitboard pawns = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Pawn);

    // white queen appears on e1
    QVERIFY(BOp::GetBit(queens, 4));

    // pawn on f2 removed
    QVERIFY(!BOp::GetBit(pawns, 13));

    // black rook on e1 removed
    QVERIFY(!BOp::GetBit(black_rooks, 4));
}

// === Standard Moves & Captures ===

void PositionTest::MoveShouldUpdateBoardCorrectly() {
    // White rook on a1 (square 0), will move to a4 (square 24)
    Position p("8/8/8/8/8/8/8/R7", Position::NONE, false, false, false, false, 0);

    Move m(0, 24,
           static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::Default);

    p.ApplyMove(m);

    Bitboard bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Rook);

    // rook no longer on a1
    QVERIFY(!BOp::GetBit(bb, 0));

    // rook now on a4
    QVERIFY(BOp::GetBit(bb, 24));
}

void PositionTest::MoveShouldHandleCaptureCorrectly() {
    // White rook on a1 (0), black knight on a4 (24)
    Position p("8/8/8/8/8/8/8/R7", Position::NONE, false, false, false, false, 0);
    p.AddPiece(24, static_cast<uint8_t>(PieceType::Knight), static_cast<uint8_t>(Side::Black));

    Move m(0, 24,
           static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White),
           static_cast<uint8_t>(PieceType::Knight), static_cast<uint8_t>(Side::Black),
           Move::Flag::Capture);

    p.ApplyMove(m);

    Bitboard rook_bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Rook);
    Bitboard knight_bb = p.GetPieces().GetPieceBitboard(Side::Black, PieceType::Knight);

    // rook captured the knight and landed on a4
    QVERIFY(BOp::GetBit(rook_bb, 24));

    // knight removed from a4
    QVERIFY(!BOp::GetBit(knight_bb, 24));
}

void PositionTest::MoveShouldHandleCastlingCorrectly() {
    // White king on e1 (4), white rook on h1 (7) → short castling
    Position p("r3k2r/8/8/8/8/8/8/R3K2R", Position::NONE,
               true, true, true, true, 0);

    Move m(4, 6,
           static_cast<uint8_t>(PieceType::King), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::WhiteShortCastling);

    p.ApplyMove(m);

    Bitboard king_bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::King);
    Bitboard rook_bb = p.GetPieces().GetPieceBitboard(Side::White, PieceType::Rook);

    // king landed on g1 (6), rook moved to f1 (5)
    QVERIFY(BOp::GetBit(king_bb, 6));
    QVERIFY(BOp::GetBit(rook_bb, 5));

    // castling flags must be cleared
    QVERIFY(!p.GetWhiteShortCastling());
    QVERIFY(!p.GetWhiteLongCastling());
}

// === Repetition & Hash ===

void PositionTest::ShouldDetectThreefoldRepetition() {
    // Empty board, hash repeated 3 times manually
    Position p("8/8/8/8/8/8/8/8", Position::NONE, false, false, false, false, 0);

    ZobristHash h = p.GetHash();
    p.repetition_history_.AddPosition(h);
    p.repetition_history_.AddPosition(h);
    p.repetition_history_.AddPosition(h);

    // Threefold repetition must be detected
    QVERIFY(p.IsThreefoldRepetition());
}

void PositionTest::HashShouldChangeOnPieceChanges() {
    // Place and remove a piece to verify hash changes and reverts
    Position p("8/8/8/8/8/8/8/8", Position::NONE, false, false, false, false, 0);
    uint64_t original = p.GetHash().GetValue();

    // Add white pawn on a1
    p.AddPiece(0, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White));
    uint64_t changed = p.GetHash().GetValue();
    QVERIFY(original != changed);

    // Remove it — hash should return to original
    p.RemovePiece(0, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White));
    uint64_t restored = p.GetHash().GetValue();
    QCOMPARE(original, restored);
}

void PositionTest::HashShouldInvertOnEachMove() {
    // Verify that a regular move changes the Zobrist hash
    Position p("8/8/8/8/8/8/4P3/8", Position::NONE, false, false, false, false, 0);
    uint64_t original_hash = p.GetHash().GetValue();

    // White pawn e2 (12) → e3 (20)
    Move m(12, 20,
           static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
           Move::None, Move::None, Move::Flag::Default);

    p.ApplyMove(m);
    uint64_t moved_hash = p.GetHash().GetValue();

    QVERIFY(original_hash != moved_hash);
}

// === Edge & Invalid Cases ===

void PositionTest::MoveShouldNotChangeBoardOnInvalidMove() {
    // White pawn is on e2 (square 12), but we try moving from d3 (20) — which is empty
    Position p("8/8/8/8/8/8/4P3/8", Position::NONE, false, false, false, false, 0);

    // Invalid move: from 20 → 28 (no piece at from-square)
    Move invalid_move(20, 28,
                      static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(Side::White),
                      Move::None, Move::None, Move::Flag::Default);

    Bitboard before = p.GetPieces().GetAllBitboard();

    p.ApplyMove(invalid_move);  // Should do nothing

    Bitboard after = p.GetPieces().GetAllBitboard();

    // Board must remain unchanged
    QCOMPARE(before, after);
}
