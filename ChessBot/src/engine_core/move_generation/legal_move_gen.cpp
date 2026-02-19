#include "legal_move_gen.h"

#include "ps_legal_move_mask_gen.h"
#include "pawn_attack_masks.h"

// Fast lookup of defender piece type on square 'sq' for side def_side (or Move::None if empty)
static inline uint8_t DefenderTypeAt(const Pieces& pcs, Side def_side, uint8_t sq) {
    for (uint8_t pt = 0; pt < static_cast<uint8_t>(PieceType::Count); ++pt) {
        if (BOp::GetBit(pcs.GetPieceBitboard(def_side, static_cast<PieceType>(pt)), sq)) {
            return pt;
        }
    }
    return Move::None;
}

// Adds a move (with legality check and promotion handling if needed)
void LegalMoveGen::TryPushMove(const Pieces& pcs, MoveList& out,
                               uint8_t from, uint8_t to,
                               PieceType attacker_type, Side attacker_side,
                               uint8_t defender_type, uint8_t defender_side,
                               Move::Flag flag)
{
    Move m{from, to, static_cast<uint8_t>(attacker_type),
           static_cast<uint8_t>(attacker_side), defender_type, defender_side, flag};

    if (!LegalMoveGen::IsLegalAfterMove(pcs, m)) {
        return;
    }

    // Promotion: if 'to' is on the last rank
    if (attacker_type == PieceType::Pawn && (to < 8 || to > 55)) {
        out.Push({from, to, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(attacker_side),
                  defender_type, defender_side, Move::Flag::PromoteToKnight});
        out.Push({from, to, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(attacker_side),
                  defender_type, defender_side, Move::Flag::PromoteToBishop});
        out.Push({from, to, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(attacker_side),
                  defender_type, defender_side, Move::Flag::PromoteToRook});
        out.Push({from, to, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(attacker_side),
                  defender_type, defender_side, Move::Flag::PromoteToQueen});
        return;
    }

    out.Push(m);
}

/*──────────────── Pawns ─────────────────*/

// Pawn captures: iterate pawns
void LegalMoveGen::GenPawnCaptures(const Pieces& pcs, Side side, MoveList& out) {
    const Side enemy = Pieces::Inverse(side);
    Bitboard pawns = pcs.GetPieceBitboard(side, PieceType::Pawn);

    while (pawns) {
        uint8_t from = BOp::BitScanForward(pawns);
        pawns = BOp::Set_0(pawns, from);

        Bitboard att = PawnMasks::kAttack[static_cast<int>(side)][from] & pcs.GetSideBoard(enemy);
        while (att) {
            uint8_t to = BOp::BitScanForward(att);
            att = BOp::Set_0(att, to);

            const uint8_t def_type = DefenderTypeAt(pcs, enemy, to);
            // paranoia, but cheap
            if (def_type == Move::None) {
                continue;
            }

            TryPushMove(pcs, out, from, to, PieceType::Pawn, side,
                        def_type, static_cast<uint8_t>(enemy), Move::Flag::Capture);
        }
    }
}

// Single/double pawn pushes: use precomputed 'to' masks from PsLegalMaskGen and restore 'from'
void LegalMoveGen::GenPawnPushes(const Pieces& pcs, Side side, MoveList& out) {
    // single: to = from ± 8
    Bitboard single_to = PsLegalMaskGen::PawnSinglePush(pcs, side);
    int8_t d1 = (side == Side::White) ? -8 : 8;

    while (single_to) {
        uint8_t to = BOp::BitScanForward(single_to);
        single_to = BOp::Set_0(single_to, to);
        uint8_t from = static_cast<uint8_t>(to + d1);

        if (!BOp::GetBit(pcs.GetPieceBitboard(side, PieceType::Pawn), from)) {
            continue;
        }

        TryPushMove(pcs, out, from, to, PieceType::Pawn, side,
                    Move::None, Move::None, Move::Flag::Default);
    }

    // double: to = from ± 16
    Bitboard dbl_to = PsLegalMaskGen::PawnDoublePush(pcs, side);
    int8_t d2 = (side == Side::White) ? -16 : 16;

    while (dbl_to) {
        uint8_t to = BOp::BitScanForward(dbl_to);
        dbl_to = BOp::Set_0(dbl_to, to);
        uint8_t from = static_cast<uint8_t>(to + d2);

        if (!BOp::GetBit(pcs.GetPieceBitboard(side, PieceType::Pawn), from)) {
            continue;
        }

        TryPushMove(pcs, out, from, to, PieceType::Pawn, side,
                    Move::None, Move::None, Move::Flag::PawnLongMove);
    }
}

/*──────────────── Converter for non-pawn pieces ───────────────*/

void LegalMoveGen::PiecesMaskToMoves(const Pieces& pcs, Bitboard to_mask,
                                     uint8_t from_sq, PieceType attacker_type,
                                     Side attacker_side, MoveList& out)
{
    // Extra safety: there must be such a piece on from_sq
    if (!BOp::GetBit(pcs.GetPieceBitboard(attacker_side, attacker_type), from_sq))
        return;

    const Side enemy = Pieces::Inverse(attacker_side);

    while (to_mask) {
        uint8_t to = BOp::BitScanForward(to_mask);
        to_mask = BOp::Set_0(to_mask, to);

        const uint8_t def_type = DefenderTypeAt(pcs, enemy, to);
        const uint8_t def_side = (def_type == Move::None) ? Move::None : static_cast<uint8_t>(enemy);

        TryPushMove(pcs, out, from_sq, to, attacker_type, attacker_side,
                    def_type, def_side,
                    (def_type == Move::None) ? Move::Flag::Default : Move::Flag::Capture);
    }
}

/*──────────────── Legality ───────────────*/

bool LegalMoveGen::IsLegalAfterMove(Pieces pcs, const Move& move) {
    const Side aSide      = static_cast<Side>(move.GetAttackerSide());
    const PieceType aType = static_cast<PieceType>(move.GetAttackerType());
    const auto flag       = move.GetFlag();

    // The attacking piece must stand on 'from'
    if (!BOp::GetBit(pcs.GetPieceBitboard(aSide, aType), move.GetFrom())) {
        return false;
    }

    // 1) Move the attacking piece (from -> to)
    {
        Bitboard bb = pcs.GetPieceBitboard(aSide, aType);
        bb = BOp::Set_0(bb, move.GetFrom());
        bb = BOp::Set_1(bb, move.GetTo());
        pcs.SetPieceBitboard(aSide, aType, bb);
    }

    // 2) Normal capture: remove opponent's piece from 'to'
    if (move.GetDefenderType() != Move::None && flag != Move::Flag::EnPassantCapture) {
        const Side dSide = static_cast<Side>(move.GetDefenderSide());
        const PieceType dType = static_cast<PieceType>(move.GetDefenderType());
        Bitboard dbb = pcs.GetPieceBitboard(dSide, dType);
        dbb = BOp::Set_0(dbb, move.GetTo());
        pcs.SetPieceBitboard(dSide, dType, dbb);
    }

    // 3) En-passant: remove the pawn from the square behind 'to'
    if (flag == Move::Flag::EnPassantCapture) {
        if (aSide == Side::White) {
            Bitboard bp = pcs.GetPieceBitboard(Side::Black, PieceType::Pawn);
            bp = BOp::Set_0(bp, move.GetTo() - 8);
            pcs.SetPieceBitboard(Side::Black, PieceType::Pawn, bp);
        } else {
            Bitboard wp = pcs.GetPieceBitboard(Side::White, PieceType::Pawn);
            wp = BOp::Set_0(wp, move.GetTo() + 8);
            pcs.SetPieceBitboard(Side::White, PieceType::Pawn, wp);
        }
    }

    // 4) Castling: move the rook (only piece moves; checks are handled during generation)
    if (aType == PieceType::King) {
        if (flag == Move::Flag::WhiteShortCastling || flag == Move::Flag::BlackShortCastling) {
            const uint8_t rf = (aSide == Side::White) ? 7  : 63;
            const uint8_t rt = (aSide == Side::White) ? 5  : 61;
            Bitboard rb = pcs.GetPieceBitboard(aSide, PieceType::Rook);
            rb = BOp::Set_0(rb, rf);
            rb = BOp::Set_1(rb, rt);
            pcs.SetPieceBitboard(aSide, PieceType::Rook, rb);
        } else if (flag == Move::Flag::WhiteLongCastling || flag == Move::Flag::BlackLongCastling) {
            const uint8_t rf = (aSide == Side::White) ? 0  : 56;
            const uint8_t rt = (aSide == Side::White) ? 3  : 59;
            Bitboard rb = pcs.GetPieceBitboard(aSide, PieceType::Rook);
            rb = BOp::Set_0(rb, rf);
            rb = BOp::Set_1(rb, rt);
            pcs.SetPieceBitboard(aSide, PieceType::Rook, rb);
        }
    }

    // 5) Promotion: remove pawn on 'to' and place the promoted piece
    if (flag == Move::Flag::PromoteToKnight || flag == Move::Flag::PromoteToBishop ||
        flag == Move::Flag::PromoteToRook   || flag == Move::Flag::PromoteToQueen) {

        // Remove pawn
        Bitboard pb = pcs.GetPieceBitboard(aSide, PieceType::Pawn);
        pb = BOp::Set_0(pb, move.GetTo());
        pcs.SetPieceBitboard(aSide, PieceType::Pawn, pb);

        const PieceType promo =
            (flag == Move::Flag::PromoteToKnight) ? PieceType::Knight :
                (flag == Move::Flag::PromoteToBishop) ? PieceType::Bishop :
                (flag == Move::Flag::PromoteToRook)   ? PieceType::Rook   : PieceType::Queen;

        Bitboard nb = pcs.GetPieceBitboard(aSide, promo);
        nb = BOp::Set_1(nb, move.GetTo());
        pcs.SetPieceBitboard(aSide, promo, nb);
    }

    pcs.UpdateBitboard();

    // 6) Own king must not be in check after the move
    const uint8_t ksq = BOp::BitScanForward(pcs.GetPieceBitboard(aSide, PieceType::King));
    return !PsLegalMaskGen::SquareInDanger(pcs, ksq, aSide);
}

/*──────────────── EP & Castling ───────────────*/

void LegalMoveGen::AddEnPassantCaptures(const Pieces& pcs, Side side, uint8_t ep_square, MoveList& out) {
    if (ep_square == Position::NONE) return;

    if (side == Side::White) {
        if (ep_square % 8 != 7 && BOp::GetBit(pcs.GetPieceBitboard(Side::White, PieceType::Pawn), ep_square - 7)) {
            TryPushMove(pcs, out, static_cast<uint8_t>(ep_square - 7), ep_square,
                        PieceType::Pawn, Side::White, Move::None, Move::None, Move::Flag::EnPassantCapture);
        }
        if (ep_square % 8 != 0 && BOp::GetBit(pcs.GetPieceBitboard(Side::White, PieceType::Pawn), ep_square - 9)) {
            TryPushMove(pcs, out, static_cast<uint8_t>(ep_square - 9), ep_square,
                        PieceType::Pawn, Side::White, Move::None, Move::None, Move::Flag::EnPassantCapture);
        }
    } else {
        if (ep_square % 8 != 0 && BOp::GetBit(pcs.GetPieceBitboard(Side::Black, PieceType::Pawn), ep_square + 7)) {
            TryPushMove(pcs, out, static_cast<uint8_t>(ep_square + 7), ep_square,
                        PieceType::Pawn, Side::Black, Move::None, Move::None, Move::Flag::EnPassantCapture);
        }
        if (ep_square % 8 != 7 && BOp::GetBit(pcs.GetPieceBitboard(Side::Black, PieceType::Pawn), ep_square + 9)) {
            TryPushMove(pcs, out, static_cast<uint8_t>(ep_square + 9), ep_square,
                        PieceType::Pawn, Side::Black, Move::None, Move::None, Move::Flag::EnPassantCapture);
        }
    }
}

void LegalMoveGen::AddCastlingMoves(const Pieces& pcs, Side side,
                                    bool long_castle, bool short_castle, MoveList& out) {
    const uint8_t base = (side == Side::White) ? 0 : 56;
    const Move::Flag long_flag  = (side == Side::White) ? Move::Flag::WhiteLongCastling  : Move::Flag::BlackLongCastling;
    const Move::Flag short_flag = (side == Side::White) ? Move::Flag::WhiteShortCastling : Move::Flag::BlackShortCastling;

    const uint8_t king_from = BOp::BitScanForward(pcs.GetPieceBitboard(side, PieceType::King));

    // do not generate castling if the king is not on e1/e8
    if (king_from != uint8_t(base + 4)) {
        return;
    }

    // O-O-O
    if (long_castle &&
        BOp::GetBit(pcs.GetPieceBitboard(side, PieceType::Rook), base + 0) &&
        BOp::GetBit(pcs.GetEmptyBitboard(), base + 1) &&
        BOp::GetBit(pcs.GetEmptyBitboard(), base + 2) &&
        BOp::GetBit(pcs.GetEmptyBitboard(), base + 3) &&
        !PsLegalMaskGen::SquareInDanger(pcs, king_from, side) &&
        !PsLegalMaskGen::SquareInDanger(pcs, base + 3, side) &&
        !PsLegalMaskGen::SquareInDanger(pcs, base + 2, side)) {

        TryPushMove(pcs, out, uint8_t(base+4), uint8_t(base+2),
                    PieceType::King, side, Move::None, Move::None, long_flag);
    }

    // O-O
    if (short_castle &&
        BOp::GetBit(pcs.GetPieceBitboard(side, PieceType::Rook), base + 7) &&
        BOp::GetBit(pcs.GetEmptyBitboard(), base + 5) &&
        BOp::GetBit(pcs.GetEmptyBitboard(), base + 6) &&
        !PsLegalMaskGen::SquareInDanger(pcs, king_from, side) &&
        !PsLegalMaskGen::SquareInDanger(pcs, base + 5, side) &&
        !PsLegalMaskGen::SquareInDanger(pcs, base + 6, side)) {

        TryPushMove(pcs, out, uint8_t(base+4), uint8_t(base+6),
                    PieceType::King, side, Move::None, Move::None, short_flag);
    }
}

/*──────────────── Entry point ───────────────*/

void LegalMoveGen::Generate(const Position& position, Side side, MoveList& out, bool only_captures) {
    out = MoveList{}; // reset
    const Pieces& pcs = position.GetPieces();

    // Pawns
    GenPawnCaptures(pcs, side, out);
    if (!only_captures)
        GenPawnPushes(pcs, side, out);

    // Knights
    Bitboard knights = pcs.GetPieceBitboard(side, PieceType::Knight);
    while (knights) {
        uint8_t from = BOp::BitScanForward(knights);
        knights = BOp::Set_0(knights, from);
        Bitboard mask = PsLegalMaskGen::KnightMask(pcs, from, side, only_captures);
        PiecesMaskToMoves(pcs, mask, from, PieceType::Knight, side, out);
    }

    // Bishops
    Bitboard bishops = pcs.GetPieceBitboard(side, PieceType::Bishop);
    while (bishops) {
        uint8_t from = BOp::BitScanForward(bishops);
        bishops = BOp::Set_0(bishops, from);
        Bitboard mask = PsLegalMaskGen::BishopMask(pcs, from, side, only_captures);
        PiecesMaskToMoves(pcs, mask, from, PieceType::Bishop, side, out);
    }

    // Rooks
    Bitboard rooks = pcs.GetPieceBitboard(side, PieceType::Rook);
    while (rooks) {
        uint8_t from = BOp::BitScanForward(rooks);
        rooks = BOp::Set_0(rooks, from);
        Bitboard mask = PsLegalMaskGen::RookMask(pcs, from, side, only_captures);
        PiecesMaskToMoves(pcs, mask, from, PieceType::Rook, side, out);
    }

    // Queens
    Bitboard queens = pcs.GetPieceBitboard(side, PieceType::Queen);
    while (queens) {
        uint8_t from = BOp::BitScanForward(queens);
        queens = BOp::Set_0(queens, from);
        Bitboard mask = PsLegalMaskGen::QueenMask(pcs, from, side, only_captures);
        PiecesMaskToMoves(pcs, mask, from, PieceType::Queen, side, out);
    }

    // King
    uint8_t k_from = BOp::BitScanForward(pcs.GetPieceBitboard(side, PieceType::King));
    Bitboard k_mask = PsLegalMaskGen::KingMask(pcs, k_from, side, only_captures);
    PiecesMaskToMoves(pcs, k_mask, k_from, PieceType::King, side, out);

    // En-Passant
    AddEnPassantCaptures(pcs, side, position.GetEnPassantSquare(), out);

    // Castling
    if (!only_captures) {
        if (side == Side::White) {
            AddCastlingMoves(pcs, Side::White, position.GetWhiteLongCastling(), position.GetWhiteShortCastling(), out);
        } else {
            AddCastlingMoves(pcs, Side::Black, position.GetBlackLongCastling(), position.GetBlackShortCastling(), out);
        }
    }
}
