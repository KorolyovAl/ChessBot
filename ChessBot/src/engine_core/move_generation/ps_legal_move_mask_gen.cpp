#include "king_masks.h"
#include "knight_masks.h"
#include "pawn_attack_masks.h"

#include "ps_legal_move_mask_gen.h"

/*─────────────────── Pawn helpers ───────────────────*/

Bitboard PsLegalMaskGen::PawnSinglePush(const Pieces& pcs, Side s) {
    Bitboard pawns = pcs.GetPieceBitboard(s, PieceType::Pawn);
    Bitboard empty = pcs.GetEmptyBitboard();

    return (s == Side::White)
               ? (pawns << 8) & empty
               : (pawns >> 8) & empty;
}

Bitboard PsLegalMaskGen::PawnDoublePush(const Pieces& pcs, Side s) {
    Bitboard single = PawnSinglePush(pcs, s);
    Bitboard empty  = pcs.GetEmptyBitboard();

    return (s == Side::White)
               ? ((single & BRows::Rows[2]) << 8) & empty   // rank‑4
               : ((single & BRows::Rows[5]) >> 8) & empty;  // rank‑5
}

Bitboard PsLegalMaskGen::PawnCaptureMask(const Pieces& pcs, Side s, bool include_all_attacks) {
    Bitboard pawns  = pcs.GetPieceBitboard(s, PieceType::Pawn);
    Bitboard enemy  = pcs.GetSideBoard(Pieces::Inverse(s));
    Bitboard result = 0;

    while (pawns) {
        uint8_t sq = BOp::BitScanForward(pawns);
        result |= PawnMasks::kAttack[static_cast<int>(s)][sq];
        pawns  = BOp::Set_0(pawns, sq);
    }

    return include_all_attacks ? result : (result & enemy);
}

/*────────────── Piece masks ──────────────*/

Bitboard PsLegalMaskGen::KingMask(const Pieces& pcs, uint8_t sq, Side s, bool only_captures) {
    return only_captures
               ? KingMasks::kMasks[sq] & pcs.GetSideBoard(Pieces::Inverse(s))
               : KingMasks::kMasks[sq] & pcs.GetInvSideBitboard(s);
}

Bitboard PsLegalMaskGen::KnightMask(const Pieces& pcs, uint8_t sq, Side s, bool only_captures) {
    return only_captures
               ? KnightMasks::kMasks[sq] & pcs.GetSideBoard(Pieces::Inverse(s))
               : KnightMasks::kMasks[sq] & pcs.GetInvSideBitboard(s);
}

Bitboard PsLegalMaskGen::RayUntilBlock(const Pieces& pcs, uint8_t sq, Side s,
                                       bool only_captures,
                                       SlidersMasks::Direction dir, bool reverse) {

    Bitboard ray = SlidersMasks::kMasks[sq][dir];
    Bitboard blockers = ray & pcs.GetAllBitboard();

    if (blockers) {
        uint8_t block_sq = reverse ? BOp::BitScanReverse(blockers) : BOp::BitScanForward(blockers);
        ray ^= SlidersMasks::kMasks[block_sq][dir];             // trim beyond blocker

        if (BOp::GetBit(pcs.GetSideBoard(s), block_sq)) {
            ray = BOp::Set_0(ray, block_sq);
        }
        // own piece
        else {
            ray = BOp::Set_1(ray, block_sq);                    // capturable
        }
    }

    return only_captures ? (ray & pcs.GetSideBoard(Pieces::Inverse(s)))
                         : ray;
}

Bitboard PsLegalMaskGen::BishopMask(const Pieces& pcs, uint8_t sq, Side s, bool only_captures) {
    using D = SlidersMasks::Direction;
    return  RayUntilBlock(pcs, sq, s, only_captures, D::NorthWest, false) |
           RayUntilBlock(pcs, sq, s, only_captures, D::NorthEast, false) |
           RayUntilBlock(pcs, sq, s, only_captures, D::SouthWest, true ) |
           RayUntilBlock(pcs, sq, s, only_captures, D::SouthEast, true );
}

Bitboard PsLegalMaskGen::RookMask(const Pieces& pcs, uint8_t sq, Side s, bool only_captures) {
    using D = SlidersMasks::Direction;
    return  RayUntilBlock(pcs, sq, s, only_captures, D::North, false) |
           RayUntilBlock(pcs, sq, s, only_captures, D::South, true ) |
           RayUntilBlock(pcs, sq, s, only_captures, D::West , true ) |
           RayUntilBlock(pcs, sq, s, only_captures, D::East , false);
}

Bitboard PsLegalMaskGen::QueenMask(const Pieces& pcs, uint8_t sq, Side s, bool only_captures) {
    return BishopMask(pcs, sq, s, only_captures) |
           RookMask  (pcs, sq, s, only_captures);
}

/*────────────── King safety ──────────────*/

bool PsLegalMaskGen::SquareInDanger(const Pieces& pcs, uint8_t sq, Side s) {
    Side enemy = Pieces::Inverse(s);

    Bitboard enemyPawns = pcs.GetPieceBitboard(enemy, PieceType::Pawn);

    // Iterate over all enemy pawns
    while (enemyPawns) {
        // pawn square index
        uint8_t pawnSq = BOp::BitScanForward(enemyPawns);
        // its attack mask
        Bitboard pawnAtt =
            PawnMasks::kAttack[static_cast<int>(enemy)][pawnSq];

        // does it attack square sq?
        if (pawnAtt & (1ULL << sq)) {
            return true;
        }

        // move to the next pawn
        enemyPawns = BOp::Set_0(enemyPawns, pawnSq);
    }

    // if (PawnMasks::kAttack[static_cast<int>(enemy)][sq] &
    //     pcs.GetPieceBitboard(enemy, PieceType::Pawn)) {
    //     return true;
    // }

    if (KnightMask(pcs, sq, s, true) &
        pcs.GetPieceBitboard(enemy, PieceType::Knight)) {
        return true;
    }

    if (BishopMask(pcs, sq, s, true) &
        pcs.GetPieceBitboard(enemy, PieceType::Bishop)) {
        return true;
    }

    if (RookMask(pcs, sq, s, true) &
        pcs.GetPieceBitboard(enemy, PieceType::Rook)) {
        return true;
    }

    if (QueenMask(pcs, sq, s, true) &
        pcs.GetPieceBitboard(enemy, PieceType::Queen)) {
        return true;
    }

    if (KingMask(pcs, sq, s, true) &
        pcs.GetPieceBitboard(enemy, PieceType::King)) {
        return true;
    }

    return false;
}
