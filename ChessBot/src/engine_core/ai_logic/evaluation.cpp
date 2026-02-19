#include "evaluation.h"
#include "static_exchange_evaluation.h"
#include "../board_state/bitboard.h"
#include "pst_tables.h"
#include "../move_generation/ps_legal_move_mask_gen.h"
#include "../move_generation/king_masks.h"
#include "../move_generation/knight_masks.h"
#include "../move_generation/sliders_masks.h"
#include "piece_values.h"

#include <array>

namespace {

// Small, safe global bonuses
constexpr int kTempoBonus       = 10;
constexpr int kBishopPairBonus  = 30;

// Game-phase weights (pawns excluded); max phase with full set (no pawns) for both sides = 24
constexpr int kPhaseWeightKnight = 1;
constexpr int kPhaseWeightBishop = 1;
constexpr int kPhaseWeightRook   = 2;
constexpr int kPhaseWeightQueen  = 4;
constexpr int kMaxPhase          = 24;

// Pawn structure tuning (centipawns)
constexpr int kIsolatedPawnPenaltyMG = 15;
constexpr int kIsolatedPawnPenaltyEG = 10;

constexpr int kDoubledPawnPenaltyMG  = 10;
constexpr int kDoubledPawnPenaltyEG  = 8;

// Passed-pawn bonuses by rank progress (0..7)
// For White ranks are used as-is; for Black progress is mirrored as (7 - rank)
constexpr int kPassedPawnBonusMG[8] = { 0, 5, 10, 20, 35, 60, 90, 0 };
constexpr int kPassedPawnBonusEG[8] = { 0, 8, 15, 30, 50, 80, 120, 0 };

// Column (file) masks A..H
inline constexpr Bitboard kColumnMask[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
};

// Mobility weights (centipawns per reachable square) in middlegame
// In MG king mobility is excluded and discouraged via king PST
constexpr int kMobKnightMG = 4;
constexpr int kMobBishopMG = 4;
constexpr int kMobRookMG   = 2;
constexpr int kMobQueenMG  = 1;

// Mobility weights in endgame; king activity is rewarded here
constexpr int kMobKnightEG = 5;
constexpr int kMobBishopEG = 5;
constexpr int kMobRookEG   = 3;
constexpr int kMobQueenEG  = 2;
constexpr int kMobKingEG   = 2;

// King safety (MG) pawn shield directly in front of the king (two ranks ahead across three columns)
constexpr int kShieldMissingRank1 = 12;   // missing pawn on first shield rank
constexpr int kShieldMissingRank2 = 8;    // missing pawn on second shield rank

// Open or half-open columns near the king (king column +/- 1)
constexpr int kNearColumnOpenPenalty     = 6;   // open column near king
constexpr int kNearColumnHalfOpenPenalty = 10;  // half-open column near king

// Local snapshot of bitboards for hypothetical exchanges
struct BoardSnapshot {
    std::array<std::array<Bitboard, PieceType::Count>, 2> piece_bb; // [side][piece]
    Bitboard occ_white;
    Bitboard occ_black;
    Bitboard occ_all;
};

// Build a snapshot from current Pieces
inline BoardSnapshot MakeSnapshot(const Pieces& pieces) {
    BoardSnapshot snapshot;
    snapshot.piece_bb  = pieces.GetPieceBitboards();
    snapshot.occ_white = pieces.GetSideBoard(Side::White);
    snapshot.occ_black = pieces.GetSideBoard(Side::Black);
    snapshot.occ_all   = pieces.GetAllBitboard();
    return snapshot;
}

// Helpers for board coordinates (0..7)
inline int ColumnOf(uint8_t sq) {
    const int col = static_cast<int>(sq & 7);
    return col;
}

inline int RankOf(uint8_t sq) {
    const int rank = static_cast<int>(sq >> 3);
    return rank;
}

// Mirror square by ranks (a1 <-> a8 etc.) assuming a1 = 0 indexing
inline uint8_t MirrorSquare(uint8_t sq) {
    const uint8_t mirrored = static_cast<uint8_t>(sq ^ 56);
    return mirrored;
}

// PST lookup for middlegame; White as-is, Black mirrored by ranks
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

// PST lookup for endgame; White as-is, Black mirrored by ranks
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

// Checks if from and to lie on the same ray (diagonal, vertical, horizontal) and that ray is clear
// interpose_out is filled with squares between from and to (excluding from and to)
static inline bool InterposeSquaresIfClear(const BoardSnapshot& s, uint8_t from, uint8_t to, Bitboard& interpose_out) {
    using D = SlidersMasks::Direction;

    for (int dir = 0; dir < static_cast<int>(D::Count); ++dir) {
        Bitboard ray = SlidersMasks::kMasks[from][dir];

        if (!BOp::GetBit(ray, to)) {
            continue;
        }

        Bitboard blockers = ray & s.occ_all;
        if (blockers) {
            uint8_t b = BOp::BitScanForward(blockers);

            if (b != to) {
                return false;
            }
        }

        // All squares on the ray from from to to excluding to
        interpose_out = ray ^ SlidersMasks::kMasks[to][dir];

        // Remove squares beyond to so only interposing squares remain
        interpose_out &= ~SlidersMasks::kMasks[to][dir];

        return true;
    }

    return false;
}

// Returns true if the defending side has a cheap way to interpose on any of the given squares
static inline bool HasCheapInterposition(const Pieces& pcs, Bitboard interpose, Side defender) {
    if (!interpose) {
        return false;
    }

    const int ds = static_cast<int>(defender);

    // Pawns: single or double push into an empty interpose square
    {
        Bitboard pawns = pcs.GetPieceBitboard(defender, PieceType::Pawn);
        if (pawns) {
            Bitboard m = interpose;
            while (m) {
                uint8_t sq = BOp::BitScanForward(m);
                m = BOp::Set_0(m, sq);

                if (BOp::GetBit(pcs.GetAllBitboard(), sq)) {
                    continue;
                }

                const int step = (defender == Side::White ? +8 : -8);
                const int from1 = static_cast<int>(sq) - step;
                if (from1 >= 0 && from1 < 64 &&
                    BOp::GetBit(pawns, static_cast<uint8_t>(from1)) &&
                    !BOp::GetBit(pcs.GetAllBitboard(), sq)) {
                    return true;
                }

                const int rank_start = (defender == Side::White ? 1 : 6);
                const int from2 = static_cast<int>(sq) - 2 * step;
                const int mid   = static_cast<int>(sq) - step;
                if (from2 >= 0 && from2 < 64 &&
                    (from2 / 8) == rank_start &&
                    BOp::GetBit(pawns, static_cast<uint8_t>(from2)) &&
                    !BOp::GetBit(pcs.GetAllBitboard(), static_cast<uint8_t>(mid)) &&
                    !BOp::GetBit(pcs.GetAllBitboard(), sq)) {
                    return true;
                }
            }
        }
    }

    // Knights: any knight that can jump to an interpose square
    {
        Bitboard knights = pcs.GetPieceBitboard(defender, PieceType::Knight);
        while (knights) {
            uint8_t n = BOp::BitScanForward(knights);
            knights = BOp::Set_0(knights, n);
            if (KnightMasks::kMasks[n] & interpose) {
                return true;
            }
        }
    }

    // Bishops: any bishop that reaches an interpose square along a clear diagonal
    {
        Bitboard bishops = pcs.GetPieceBitboard(defender, PieceType::Bishop);
        while (bishops) {
            uint8_t b = BOp::BitScanForward(bishops);
            bishops = BOp::Set_0(bishops, b);
            if (PsLegalMaskGen::BishopMask(pcs, b, defender, /*only_captures=*/false) & interpose) {
                return true;
            }
        }
    }

    // Rooks and queens are intentionally not considered here as interposition with them is more expensive
    return false;
}

// Computes penalty for pieces that are under a real capturing threat
static int ComputeCapturingPenalty(const Position& pos) {
    const Pieces& pieces = pos.GetPieces();
    BoardSnapshot s = MakeSnapshot(pieces);
    Bitboard occ = pieces.GetAllBitboard();
    Side side_to_move = pos.IsWhiteToMove() ? Side::White : Side::Black;
    int penalty_white = 0;
    int penalty_black = 0;

    Bitboard occupied = occ;
    while (occupied) {
        const uint8_t sq = BOp::BitScanForward(occupied);
        occupied = BOp::Set_0(occupied, sq);

        auto pr = pieces.GetPiece(sq);
        Side owner = pr.first;
        PieceType piece_type = pr.second;
        if (piece_type == PieceType::None) {
            continue;
        }

        // SquareInDanger returns true if side owner attacks its own square (piece is attacked by enemy)
        if (!PsLegalMaskGen::SquareInDanger(pieces, sq, owner)) {
            continue;
        }

        Side attacker = Pieces::Inverse(owner);
        int see_for_attacker = StaticExchangeEvaluation::On(pieces, sq, owner);

        // Condition (1): side to move equals attacker and SEE is non-negative
        bool real_threat = (side_to_move == attacker && see_for_attacker >= 0);

        // Condition (2): side to move is defender and there is no cheap interposition
        if (!real_threat) {
            Bitboard enemy_sliders =
                s.piece_bb[static_cast<int>(attacker)][PieceType::Bishop]
                | s.piece_bb[static_cast<int>(attacker)][PieceType::Rook]
                | s.piece_bb[static_cast<int>(attacker)][PieceType::Queen];

            bool has_no_cheap_interpose = true;
            Bitboard tmp = enemy_sliders;
            while (tmp) {
                uint8_t from = BOp::BitScanForward(tmp);
                tmp = BOp::Set_0(tmp, from);

                Bitboard interpose = 0;
                if (!InterposeSquaresIfClear(s, from, sq, interpose)) {
                    continue;
                }

                // If the slider is adjacent, interpose is impossible
                if (!interpose) {
                    continue;
                }

                if (HasCheapInterposition(pieces, interpose, owner)) {
                    has_no_cheap_interpose = false;
                    break;
                }
            }

            if (has_no_cheap_interpose && see_for_attacker >= 0) {
                real_threat = true;
            }
        }

        if (!real_threat) {
            continue;
        }

        int base = EvalValues::kPieceValueCp[piece_type];

        int penalty_val = std::min(base, see_for_attacker + 80);
        if (piece_type == PieceType::Pawn) {
            penalty_val = std::min(base, see_for_attacker + 40);
        }

        if (owner == Side::White) {
            penalty_white += penalty_val;
        }
        else {
            penalty_black += penalty_val;
        }
    }

    return (penalty_black - penalty_white);
}

} // namespace

test::EvaluatePos Evaluation::EvaluateForTest(const Position& position) {
    const Pieces& pieces = position.GetPieces();
    test::EvaluatePos score;

    // Baseline: material and bishop pair
    score.material = ComputeMaterialScore(pieces);
    score.imbalance = ComputeBishopPairBonus(pieces);

    // Tapered terms: PST, pawns, mobility, king safety
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

    int capturing_threat = ComputeCapturingPenalty(position);
    int capturing_mg = capturing_threat;
    int capturing_eg = capturing_threat / 2;
    int total_capturing = ((capturing_mg * phase) + (capturing_eg * (kMaxPhase - phase))) / kMaxPhase;

    score.pawns_eg = pawns_eg;
    score.pawns_mg = pawns_mg;
    score.mobility_eg = mob_eg;
    score.mobility_mg = mob_mg;
    score.pst_eg = pst_eg;
    score.pst_mg = pst_mg;
    score.capturing = total_capturing;

    score.common = score.material + score.imbalance + total_capturing;
    score.common += (mg_total * phase + eg_total * (kMaxPhase - phase)) / kMaxPhase;

    // Small tempo bonus for the side to move
    if (position.IsWhiteToMove()) {
        score.common += kTempoBonus;
    } else {
        score.common -= kTempoBonus;
    }

    return score;
}

// Entry point
int Evaluation::Evaluate(const Position& position) {
    test::EvaluatePos res = EvaluateForTest(position);
    return res.common;
}

// Baseline material evaluation
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

// Phase and PST
int Evaluation::ComputeGamePhase(const Pieces& pieces) {
    int phase = 0;

    const int queens =
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Queen))) +
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Queen)));
    phase += queens * kPhaseWeightQueen;

    const int rooks =
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Rook))) +
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Rook)));
    phase += rooks * kPhaseWeightRook;

    const int bishops =
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Bishop))) +
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Bishop)));
    phase += bishops * kPhaseWeightBishop;

    const int knights =
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::White, PieceType::Knight))) +
        static_cast<int>(BOp::Count_1(pieces.GetPieceBitboard(Side::Black, PieceType::Knight)));
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

        Bitboard bb_white = pieces.GetPieceBitboard(Side::White, pt);
        while (bb_white) {
            const uint8_t sq = BOp::BitScanForward(bb_white);
            bb_white = BOp::Set_0(bb_white, sq);
            score += PST_MG_At(pt, sq, Side::White);
        }

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

        Bitboard bb_white = pieces.GetPieceBitboard(Side::White, pt);
        while (bb_white) {
            const uint8_t sq = BOp::BitScanForward(bb_white);
            bb_white = BOp::Set_0(bb_white, sq);
            score += PST_EG_At(pt, sq, Side::White);
        }

        Bitboard bb_black = pieces.GetPieceBitboard(Side::Black, pt);
        while (bb_black) {
            const uint8_t sq = BOp::BitScanForward(bb_black);
            bb_black = BOp::Set_0(bb_black, sq);
            score -= PST_EG_At(pt, sq, Side::Black);
        }
    }

    return score;
}

// Pawn structure
// Doubled and isolated pawns on each column, counted per side
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

// Checks if no enemy pawns exist ahead on the same or adjacent columns
static bool IsPassedPawn(uint8_t sq, Side side, Bitboard enemy_pawns) {
    const int col = ColumnOf(sq);
    int r = RankOf(sq);

    while (true) {
        if (side == Side::White) {
            ++r;
            if (r > 7) {
                break;
            }
        }
        else {
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

// Sums passed pawn bonuses for one side in MG and EG scales
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

    mg += ComputeDoubledIsolatedForSide(wp, kDoubledPawnPenaltyMG, kIsolatedPawnPenaltyMG);
    mg -= ComputeDoubledIsolatedForSide(bp, kDoubledPawnPenaltyMG, kIsolatedPawnPenaltyMG);

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

    eg += ComputeDoubledIsolatedForSide(wp, kDoubledPawnPenaltyEG, kIsolatedPawnPenaltyEG);
    eg -= ComputeDoubledIsolatedForSide(bp, kDoubledPawnPenaltyEG, kIsolatedPawnPenaltyEG);

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

// Mobility helpers
static int CountMobilityForSide(const Pieces& pcs,
                                Side side,
                                int wN, int wB, int wR, int wQ, int wK) {
    int score = 0;

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
    const int white =
        CountMobilityForSide(pcs, Side::White, kMobKnightMG, kMobBishopMG, kMobRookMG, kMobQueenMG, 0);
    const int black =
        CountMobilityForSide(pcs, Side::Black, kMobKnightMG, kMobBishopMG, kMobRookMG, kMobQueenMG, 0);
    return white - black;
}

int Evaluation::ComputeMobilityEG(const Position& position) {
    const Pieces& pcs = position.GetPieces();
    const int white =
        CountMobilityForSide(pcs, Side::White, kMobKnightEG, kMobBishopEG, kMobRookEG, kMobQueenEG, kMobKingEG);
    const int black =
        CountMobilityForSide(pcs, Side::Black, kMobKnightEG, kMobBishopEG, kMobRookEG, kMobQueenEG, kMobKingEG);
    return white - black;
}

// King safety (MG)
// Column openness near king (king column +/- 1) for open and half-open files against us
static int OpenColumnPenaltyNearKing(const Pieces& pieces, Side side, int king_column) {
    int penalty = 0;

    const Bitboard ownP = pieces.GetPieceBitboard(side, PieceType::Pawn);
    const Bitboard oppP = pieces.GetPieceBitboard(
        (side == Side::White) ? Side::Black : Side::White, PieceType::Pawn);

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
        }
        else if (own_empty && !opp_empty) {
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

// Penalizes attacked king-ring squares (8-neighborhood) using projected attack masks
static int KingRingDangerPenalty(const Pieces& pcs, Side side, uint8_t ksq) {
    int penalty = 0;

    Bitboard ring = KingMasks::kMasks[ksq];
    Bitboard bb = ring;

    while (bb) {
        const uint8_t sq = BOp::BitScanForward(bb);
        bb = BOp::Set_0(bb, sq);

        if (PsLegalMaskGen::SquareInDanger(pcs, sq, side)) {
            penalty += 4;
        }
    }

    return penalty;
}

int Evaluation::ComputeKingSafetyMG(const Position& position) {
    const Pieces& pcs = position.GetPieces();
    int score = 0;

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
