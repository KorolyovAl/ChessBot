#include "evaluation.h"
#include "../board_state/bitboard.h"
#include "pst_tables.h"
#include "../move_generation/ps_legal_move_mask_gen.h"
#include "../move_generation/king_masks.h"
#include "piece_values.h"

namespace {

    // Small, safe global bonuses
    constexpr int kTempoBonus       = 10;
    constexpr int kBishopPairBonus  = 30;

    // Game-phase weights (pawns excluded). Max phase with full set (no pawns) for both sides = 24.
    constexpr int kPhaseWeightKnight = 1;
    constexpr int kPhaseWeightBishop = 1;
    constexpr int kPhaseWeightRook   = 2;
    constexpr int kPhaseWeightQueen  = 4;
    constexpr int kMaxPhase          = 24;

    // --- Pawn structure tuning (centipawns) ---
    constexpr int kIsolatedPawnPenaltyMG = 15;
    constexpr int kIsolatedPawnPenaltyEG = 10;

    constexpr int kDoubledPawnPenaltyMG  = 10;
    constexpr int kDoubledPawnPenaltyEG  = 8;

    // Passed-pawn bonuses by rank progress (0..7).
    // For White we read ranks as-is; for Black we mirror progress (7 - rank).
    constexpr int kPassedPawnBonusMG[8] = { 0, 5, 10, 20, 35, 60, 90, 0 };
    constexpr int kPassedPawnBonusEG[8] = { 0, 8, 15, 30, 50, 80, 120, 0 };

    // Column (file) masks A..H
    inline constexpr Bitboard kColumnMask[8] = {
        0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
        0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
    };

    // Board helpers (0..7)
    inline int ColumnOf(uint8_t sq) {
        const int col = static_cast<int>(sq & 7);
        return col;
    }
    inline int RankOf(uint8_t sq) {
        const int rank = static_cast<int>(sq >> 3);
        return rank;
    }

    // Mirror by ranks (a1 <-> a8 etc.) assuming a1 = 0 indexing
    inline uint8_t MirrorSquare(uint8_t sq) {
        const uint8_t mirrored = static_cast<uint8_t>(sq ^ 56);
        return mirrored;
    }

    // PST lookups (MG/EG), White as-is, Black mirrored by ranks
    inline int16_t PST_MG_At(PieceType pt, uint8_t sq, Side side) {
        const uint8_t idx = (side == Side::White) ? sq : MirrorSquare(sq);
        switch (pt) {
        case PieceType::Pawn:   return PST_MG_Pawn[idx];
        case PieceType::Knight: return PST_MG_Knight[idx];
        case PieceType::Bishop: return PST_MG_Bishop[idx];
        case PieceType::Rook:   return PST_MG_Rook[idx];
        case PieceType::Queen:  return PST_MG_Queen[idx];
        case PieceType::King:   return PST_MG_King[idx];
        default:                return 0;
        }
    }
    inline int16_t PST_EG_At(PieceType pt, uint8_t sq, Side side) {
        const uint8_t idx = (side == Side::White) ? sq : MirrorSquare(sq);
        switch (pt) {
        case PieceType::Pawn:   return PST_EG_Pawn[idx];
        case PieceType::Knight: return PST_EG_Knight[idx];
        case PieceType::Bishop: return PST_EG_Bishop[idx];
        case PieceType::Rook:   return PST_EG_Rook[idx];
        case PieceType::Queen:  return PST_EG_Queen[idx];
        case PieceType::King:   return PST_EG_King[idx];
        default:                return 0;
        }
    }

    // --- Mobility weights (centipawns per reachable square) ---
    // In MG we exclude king mobility (king activity discouraged in middlegame via PST MG).
    constexpr int kMobKnightMG = 4;
    constexpr int kMobBishopMG = 4;
    constexpr int kMobRookMG   = 2;
    constexpr int kMobQueenMG  = 1;

    // In EG mobility generally matters more (including king activity).
    constexpr int kMobKnightEG = 5;
    constexpr int kMobBishopEG = 5;
    constexpr int kMobRookEG   = 3;
    constexpr int kMobQueenEG  = 2;
    constexpr int kMobKingEG   = 2;

    // --- King Safety (MG) ---
    // Pawn shield directly in front of the king (two ranks ahead across three columns).
    constexpr int kShieldMissingRank1 = 12;   // missing pawn on first shield rank
    constexpr int kShieldMissingRank2 = 8;    // missing pawn on second shield rank

    // Open/half-open columns near the king (king column +/- 1)
    constexpr int kNearColumnOpenPenalty     = 6;   // open column near king
    constexpr int kNearColumnHalfOpenPenalty = 10;  // half-open (opponent's) column near king
} // namespace

// === Entry point ===
int Evaluation::Evaluate(const Position& position) {
    const Pieces& pieces = position.GetPieces();
    int score = 0;

    // Baseline: material and bishop pair
    score += ComputeMaterialScore(pieces);
    score += ComputeBishopPairBonus(pieces);

    // Tapered terms: PST + Pawns + Mobility + KingSafety
    const int pst_mg   = ComputePieceSquareScoreMG(pieces);
    const int pst_eg   = ComputePieceSquareScoreEG(pieces);
    const int pawns_mg = ComputePawnStructureMG(pieces);
    const int pawns_eg = ComputePawnStructureEG(pieces);
    const int mob_mg   = ComputeMobilityMG(position);
    const int mob_eg   = ComputeMobilityEG(position);
    const int king_mg  = ComputeKingSafetyMG(position);

    int phase = ComputeGamePhase(pieces);
    if (phase < 0) {
        phase = 0;
    }
    if (phase > kMaxPhase) {
        phase = kMaxPhase;
    }

    const int mg_total = pst_mg + pawns_mg + mob_mg + king_mg;
    const int eg_total = pst_eg + pawns_eg + mob_eg;

    score += (mg_total * phase + eg_total * (kMaxPhase - phase)) / kMaxPhase;

    // Small tempo bonus for the side to move
    if (position.IsWhiteToMove()) {
        score += kTempoBonus;
    } else {
        score -= kTempoBonus;
    }

    return score;
}

// === Baseline ===
int Evaluation::ComputeMaterialScore(const Pieces& pieces) {
    int total = 0;

    for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
        const PieceType piece_type = static_cast<PieceType>(type_index);
        const Bitboard white_bb = pieces.GetPieceBitboard(Side::White, piece_type);
        const Bitboard black_bb = pieces.GetPieceBitboard(Side::Black, piece_type);

        const int white_count = static_cast<int>(BOp::Count_1(white_bb));
        const int black_count = static_cast<int>(BOp::Count_1(black_bb));
        const int diff = white_count - black_count;

        total += diff * EvalValues::kPieceValueCp[type_index];
    }

    return total;
}

int Evaluation::ComputeBishopPairBonus(const Pieces& pieces) {
    int bonus = 0;

    const int white_bishops = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Bishop)));
    const int black_bishops = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Bishop)));

    if (white_bishops >= 2) {
        bonus += kBishopPairBonus;
    }
    if (black_bishops >= 2) {
        bonus -= kBishopPairBonus;
    }

    return bonus;
}

// === Phase and PST ===
int Evaluation::ComputeGamePhase(const Pieces& pieces) {
    int phase = 0;

    const int queens  = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Queen)))
                       + static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Queen)));
    phase += queens * kPhaseWeightQueen;

    const int rooks   = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Rook)))
                      + static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Rook)));
    phase += rooks * kPhaseWeightRook;

    const int bishops = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Bishop)))
                        + static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Bishop)));
    phase += bishops * kPhaseWeightBishop;

    const int knights = static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Knight)))
                        + static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Knight)));
    phase += knights * kPhaseWeightKnight;

    if (phase > kMaxPhase) {
        phase = kMaxPhase;
    }

    return phase;
}

int Evaluation::ComputePieceSquareScoreMG(const Pieces& pieces) {
    int score = 0;

    for (int idx = 0; idx < static_cast<int>(PieceType::Count); ++idx) {
        const PieceType pt = static_cast<PieceType>(idx);

        // White
        Bitboard bb_white = pieces.GetPieceBitboard(Side::White, pt);
        while (bb_white) {
            const uint8_t sq = BOp::BitScanForward(bb_white);
            bb_white = BOp::Set_0(bb_white, sq);
            score += PST_MG_At(pt, sq, Side::White);
        }

        // Black
        Bitboard bb_black = pieces.GetPieceBitboard(Side::Black, pt);
        while (bb_black) {
            const uint8_t sq = BOp::BitScanForward(bb_black);
            bb_black = BOp::Set_0(bb_black, sq);
            score -= PST_MG_At(pt, sq, Side::Black);
        }
    }

    return score;
}

int Evaluation::ComputePieceSquareScoreEG(const Pieces& pieces) {
    int score = 0;

    for (int idx = 0; idx < static_cast<int>(PieceType::Count); ++idx) {
        const PieceType pt = static_cast<PieceType>(idx);

        // White
        Bitboard bb_white = pieces.GetPieceBitboard(Side::White, pt);
        while (bb_white) {
            const uint8_t sq = BOp::BitScanForward(bb_white);
            bb_white = BOp::Set_0(bb_white, sq);
            score += PST_EG_At(pt, sq, Side::White);
        }

        // Black
        Bitboard bb_black = pieces.GetPieceBitboard(Side::Black, pt);
        while (bb_black) {
            const uint8_t sq = BOp::BitScanForward(bb_black);
            bb_black = BOp::Set_0(bb_black, sq);
            score -= PST_EG_At(pt, sq, Side::Black);
        }
    }

    return score;
}

// === Pawn structure ===
// Doubled/isolated on each column, counted per side
static int ComputeDoubledIsolatedForSide(Bitboard pawns_side,
                                         int doubled_penalty,
                                         int isolated_penalty) {
    int score = 0;

    for (int c = 0; c < 8; ++c) {
        const Bitboard on_col = pawns_side & kColumnMask[c];
        const int count = static_cast<int>(BOp::Count_1(on_col));

        if (count >= 2) {
            const int extra = count - 1;
            score -= extra * doubled_penalty;
        }

        bool has_left = false;
        if (c > 0) {
            has_left = (pawns_side & kColumnMask[c - 1]) != 0;
        }

        bool has_right = false;
        if (c < 7) {
            has_right = (pawns_side & kColumnMask[c + 1]) != 0;
        }

        if (!has_left && !has_right && count > 0) {
            score -= count * isolated_penalty;
        }
    }

    return score;
}

// True if no enemy pawns exist ahead on the same or adjacent columns.
static bool IsPassedPawn(uint8_t sq, Side side, Bitboard enemy_pawns) {
    const int col = ColumnOf(sq);
    int r = RankOf(sq);

    for (;;) {
        if (side == Side::White) {
            ++r;
            if (r > 7) {
                break;
            }
        } else {
            --r;
            if (r < 0) {
                break;
            }
        }

        for (int dc = -1; dc <= 1; ++dc) {
            const int nc = col + dc;
            if (nc < 0 || nc > 7) {
                continue;
            }

            const uint8_t nsq = static_cast<uint8_t>((r << 3) | nc);
            const bool enemy_here = BOp::GetBit(enemy_pawns, nsq) != 0;

            if (enemy_here) {
                return false;
            }
        }
    }

    return true;
}

// Sum passed-pawn bonuses for one side (MG and EG scales).
static void ComputePassedBonusesForSide(Bitboard pawns_side,
                                        Bitboard enemy_pawns,
                                        Side side,
                                        const int bonus_mg[8],
                                        const int bonus_eg[8],
                                        int& out_mg,
                                        int& out_eg) {
    int mg = 0;
    int eg = 0;

    Bitboard bb = pawns_side;
    while (bb) {
        const uint8_t sq = BOp::BitScanForward(bb);
        bb = BOp::Set_0(bb, sq);

        if (IsPassedPawn(sq, side, enemy_pawns)) {
            const int rank = RankOf(sq);
            const int progress = (side == Side::White) ? rank : (7 - rank);
            mg += bonus_mg[progress];
            eg += bonus_eg[progress];
        }
    }

    out_mg = mg;
    out_eg = eg;
}

int Evaluation::ComputePawnStructureMG(const Pieces& pieces) {
    int mg = 0;

    const Bitboard wp = pieces.GetPieceBitboard(Side::White, PieceType::Pawn);
    const Bitboard bp = pieces.GetPieceBitboard(Side::Black, PieceType::Pawn);

    // Doubled / isolated
    mg += ComputeDoubledIsolatedForSide(wp, kDoubledPawnPenaltyMG, kIsolatedPawnPenaltyMG);
    mg -= ComputeDoubledIsolatedForSide(bp, kDoubledPawnPenaltyMG, kIsolatedPawnPenaltyMG);

    // Passed pawns (MG scale)
    int mg_pass_w = 0;
    int eg_dummy_w = 0;
    int mg_pass_b = 0;
    int eg_dummy_b = 0;

    ComputePassedBonusesForSide(wp, bp, Side::White, kPassedPawnBonusMG, kPassedPawnBonusMG, mg_pass_w, eg_dummy_w);
    ComputePassedBonusesForSide(bp, wp, Side::Black, kPassedPawnBonusMG, kPassedPawnBonusMG, mg_pass_b, eg_dummy_b);

    mg += mg_pass_w;
    mg -= mg_pass_b;

    return mg;
}

int Evaluation::ComputePawnStructureEG(const Pieces& pieces) {
    int eg = 0;

    const Bitboard wp = pieces.GetPieceBitboard(Side::White, PieceType::Pawn);
    const Bitboard bp = pieces.GetPieceBitboard(Side::Black, PieceType::Pawn);

    // Doubled / isolated
    eg += ComputeDoubledIsolatedForSide(wp, kDoubledPawnPenaltyEG, kIsolatedPawnPenaltyEG);
    eg -= ComputeDoubledIsolatedForSide(bp, kDoubledPawnPenaltyEG, kIsolatedPawnPenaltyEG);

    // Passed pawns (EG scale)
    int mg_dummy_w = 0;
    int eg_pass_w = 0;
    int mg_dummy_b = 0;
    int eg_pass_b = 0;

    ComputePassedBonusesForSide(wp, bp, Side::White, kPassedPawnBonusEG, kPassedPawnBonusEG, mg_dummy_w, eg_pass_w);
    ComputePassedBonusesForSide(bp, wp, Side::Black, kPassedPawnBonusEG, kPassedPawnBonusEG, mg_dummy_b, eg_pass_b);

    eg += eg_pass_w;
    eg -= eg_pass_b;

    return eg;
}

// === Mobility ===
// Uses your PsLegalMoveMaskGen masks (fast, no simulation). Pins are not removed,
// which is OK for mobility: it correlates well with activity without heavy cost.
static int CountMobilityForSide(const Pieces& pcs,
                                Side side,
                                int wN, int wB, int wR, int wQ, int wK) {
    int score = 0;

    // Knight
    {
        Bitboard bb = pcs.GetPieceBitboard(side, PieceType::Knight);
        while (bb) {
            const uint8_t sq = BOp::BitScanForward(bb);
            bb = BOp::Set_0(bb, sq);
            const Bitboard moves = PsLegalMaskGen::KnightMask(pcs, sq, side, /*only_captures=*/false);
            const int cnt = static_cast<int>(BOp::Count_1(moves));
            score += cnt * wN;
        }
    }

    // Bishop
    {
        Bitboard bb = pcs.GetPieceBitboard(side, PieceType::Bishop);
        while (bb) {
            const uint8_t sq = BOp::BitScanForward(bb);
            bb = BOp::Set_0(bb, sq);
            const Bitboard moves = PsLegalMaskGen::BishopMask(pcs, sq, side, /*only_captures=*/false);
            const int cnt = static_cast<int>(BOp::Count_1(moves));
            score += cnt * wB;
        }
    }

    // Rook
    {
        Bitboard bb = pcs.GetPieceBitboard(side, PieceType::Rook);
        while (bb) {
            const uint8_t sq = BOp::BitScanForward(bb);
            bb = BOp::Set_0(bb, sq);
            const Bitboard moves = PsLegalMaskGen::RookMask(pcs, sq, side, /*only_captures=*/false);
            const int cnt = static_cast<int>(BOp::Count_1(moves));
            score += cnt * wR;
        }
    }

    // Queen
    {
        Bitboard bb = pcs.GetPieceBitboard(side, PieceType::Queen);
        while (bb) {
            const uint8_t sq = BOp::BitScanForward(bb);
            bb = BOp::Set_0(bb, sq);
            const Bitboard moves = PsLegalMaskGen::QueenMask(pcs, sq, side, /*only_captures=*/false);
            const int cnt = static_cast<int>(BOp::Count_1(moves));
            score += cnt * wQ;
        }
    }

    // King (only if non-zero weight; used in EG)
    if (wK != 0) {
        Bitboard bb = pcs.GetPieceBitboard(side, PieceType::King);
        while (bb) {
            const uint8_t sq = BOp::BitScanForward(bb);
            bb = BOp::Set_0(bb, sq);
            const Bitboard moves = PsLegalMaskGen::KingMask(pcs, sq, side, /*only_captures=*/false);
            const int cnt = static_cast<int>(BOp::Count_1(moves));
            score += cnt * wK;
        }
    }

    return score;
}

int Evaluation::ComputeMobilityMG(const Position& position) {
    const Pieces& pcs = position.GetPieces();
    const int white = CountMobilityForSide(pcs, Side::White, kMobKnightMG, kMobBishopMG, kMobRookMG, kMobQueenMG, /*K*/0);
    const int black = CountMobilityForSide(pcs, Side::Black, kMobKnightMG, kMobBishopMG, kMobRookMG, kMobQueenMG, /*K*/0);
    return white - black;
}

int Evaluation::ComputeMobilityEG(const Position& position) {
    const Pieces& pcs = position.GetPieces();
    const int white = CountMobilityForSide(pcs, Side::White, kMobKnightEG, kMobBishopEG, kMobRookEG, kMobQueenEG, kMobKingEG);
    const int black = CountMobilityForSide(pcs, Side::Black, kMobKnightEG, kMobBishopEG, kMobRookEG, kMobQueenEG, kMobKingEG);
    return white - black;
}

// === King safety (MG) ===
// Column openness near king (king column +/- 1): open or half-open against us.
static int OpenColumnPenaltyNearKing(const Pieces& pieces, Side side, int king_column) {
    int penalty = 0;

    const Bitboard ownP = pieces.GetPieceBitboard(side, PieceType::Pawn);
    const Bitboard oppP = pieces.GetPieceBitboard((side == Side::White) ? Side::Black : Side::White, PieceType::Pawn);

    for (int dc = -1; dc <= 1; ++dc) {
        const int c = king_column + dc;
        if (c < 0 || c > 7) {
            continue;
        }

        const Bitboard own_on_col = ownP & kColumnMask[c];
        const Bitboard opp_on_col = oppP & kColumnMask[c];

        const bool own_empty = (own_on_col == 0);
        const bool opp_empty = (opp_on_col == 0);

        if (own_empty && opp_empty) {
            penalty += kNearColumnOpenPenalty;
        } else if (own_empty && !opp_empty) {
            penalty += kNearColumnHalfOpenPenalty;
        }
    }

    return penalty;
}

// Pawn shield directly in front of the king (three columns, two ranks forward)
static int PawnShieldPenalty(const Pieces& pieces, Side side, uint8_t king_sq) {
    const int kc = ColumnOf(king_sq);
    const int kr = RankOf(king_sq);
    const Bitboard pawns = pieces.GetPieceBitboard(side, PieceType::Pawn);

    int missing = 0;

    for (int dc = -1; dc <= 1; ++dc) {
        const int c = kc + dc;
        if (c < 0 || c > 7) {
            continue;
        }

        const int r1 = kr + ((side == Side::White) ? 1 : -1);
        const int r2 = kr + ((side == Side::White) ? 2 : -2);

        if (r1 >= 0 && r1 <= 7) {
            const uint8_t sq1 = static_cast<uint8_t>((r1 << 3) | c);
            const bool has_pawn1 = BOp::GetBit(pawns, sq1) != 0;
            if (!has_pawn1) {
                missing += kShieldMissingRank1;
            }
        }

        if (r2 >= 0 && r2 <= 7) {
            const uint8_t sq2 = static_cast<uint8_t>((r2 << 3) | c);
            const bool has_pawn2 = BOp::GetBit(pawns, sq2) != 0;
            if (!has_pawn2) {
                missing += kShieldMissingRank2;
            }
        }
    }

    return missing;
}

// Penalize attacked king-ring squares (8-neighborhood) using project attack masks
static int KingRingDangerPenalty(const Pieces& pcs, Side side, uint8_t ksq) {
    int penalty = 0;

    Bitboard ring = KingMasks::kMasks[ksq]; // 8 adjacent squares around the king
    Bitboard bb = ring;

    while (bb) {
        const uint8_t sq = BOp::BitScanForward(bb);
        bb = BOp::Set_0(bb, sq);

        if (PsLegalMaskGen::SquareInDanger(pcs, sq, side)) {
            penalty += 4; // light penalty per attacked ring square
        }
    }

    return penalty;
}

int Evaluation::ComputeKingSafetyMG(const Position& position) {
    const Pieces& pcs = position.GetPieces();
    int score = 0;

    // White king
    {
        const Bitboard kb = pcs.GetPieceBitboard(Side::White, PieceType::King);
        if (kb) {
            const uint8_t ksq = BOp::BitScanForward(kb);
            const int shield  = PawnShieldPenalty(pcs, Side::White, ksq);
            const int columns = OpenColumnPenaltyNearKing(pcs, Side::White, ColumnOf(ksq));
            const int ring    = KingRingDangerPenalty(pcs, Side::White, ksq);
            score -= (shield + columns + ring);
        }
    }

    // Black king (subtracting from White's perspective == adding here)
    {
        const Bitboard kb = pcs.GetPieceBitboard(Side::Black, PieceType::King);
        if (kb) {
            const uint8_t ksq = BOp::BitScanForward(kb);
            const int shield  = PawnShieldPenalty(pcs, Side::Black, ksq);
            const int columns = OpenColumnPenaltyNearKing(pcs, Side::Black, ColumnOf(ksq));
            const int ring    = KingRingDangerPenalty(pcs, Side::Black, ksq);
            score += (shield + columns + ring);
        }
    }

    return score;
}
