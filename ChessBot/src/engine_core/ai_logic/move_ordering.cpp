#include "move_ordering.h"
#include "static_exchange_evaluation.h"
#include "piece_values.h"
#include "../board_state/bitboard.h"

namespace {

// 16-bit compact key from from|to squares
inline uint16_t FromToKey(const Move& m) {
    uint16_t from = static_cast<uint16_t>(m.GetFrom());
    uint16_t to   = static_cast<uint16_t>(m.GetTo());
    uint16_t key  = static_cast<uint16_t>(from | (to << 8));
    return key;
}

// Check if key matches move by its from|to key
inline bool SameKey(uint16_t key, const Move& m) {
    if (key == 0xFFFF) {
        return false;
    }

    uint16_t k = FromToKey(m);
    if (k == key) {
        return true;
    }

    return false;
}

// Clamp integer value to [lo, hi]
inline int Clamp(int x, int lo, int hi) {
    if (x < lo) {
        return lo;
    }

    if (x > hi) {
        return hi;
    }
    return x;
}

// Get victim piece type index on target square for MVV-LVA
inline int GetVictimTypeIndex(const Pieces& pieces, Side victim_side, uint8_t target_square) {
    for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
        PieceType piece_type = static_cast<PieceType>(type_index);
        Bitboard bb = pieces.GetPieceBitboard(victim_side, piece_type);
        bool has_piece = BOp::GetBit(bb, target_square);

        if (has_piece) {
            return type_index;
        }
    }
    return -1;
}

// Check if flag corresponds to a promotion move
inline bool IsPromotionFlag(Move::Flag f) {
    switch (f) {
        case Move::Flag::PromoteToKnight: {
            return true;
        }
        case Move::Flag::PromoteToBishop: {
            return true;
        }
        case Move::Flag::PromoteToRook: {
            return true;
        }
        case Move::Flag::PromoteToQueen: {
            return true;
        }
        default: {
            return false;
        }
    }
}

// Simple move flag: not a capture and not a promotion
inline bool IsSimpleFlag(Move::Flag f) {
    if (f == Move::Flag::Capture) {
        return false;
    }

    if (f == Move::Flag::EnPassantCapture) {
        return false;
    }

    if (IsPromotionFlag(f)) {
        return false;
    }
    return true;
}

} // namespace

int MoveOrdering::Score(const Move& move, const Pieces& pieces, const Context& ctx) {
    // 1) TT move has absolute priority
    if (SameKey(FromToKey(ctx.tt_move), move)) {
        return 1000000;
    }

    // 2) Promotions and en passant are next after TT move
    Move::Flag flag = move.GetFlag();
    switch (flag) {
        case Move::Flag::PromoteToQueen: {
            return 900000;
        }
        case Move::Flag::PromoteToRook: {
            return 880000;
        }
        case Move::Flag::PromoteToBishop: {
            return 870000;
        }
        case Move::Flag::PromoteToKnight: {
            return 870000;
        }
        case Move::Flag::EnPassantCapture: {
            return 860000;
        }
        case Move::Flag::Capture: {
            // 3) Captures: MVV-LVA + SEE to separate good and losing captures
            Side victim_side =
                (move.GetAttackerSide() == static_cast<uint8_t>(Side::White)) ? Side::Black : Side::White;

            int victim_index   = GetVictimTypeIndex(pieces, victim_side, move.GetTo());
            int attacker_index = static_cast<int>(move.GetAttackerType());

            int mvv_lva = 0;
            if (victim_index >= 0) {
                int victim_value     = EvalValues::kPieceValueCp[victim_index];
                int attacker_penalty = (attacker_index == PieceType::King)
                                           ? 10
                                           : EvalValues::kPieceValueCp[attacker_index];
                mvv_lva = victim_value - attacker_penalty;
            }

            int see_value = StaticExchangeEvaluation::Capture(pieces, move);
            int score = 500000 + mvv_lva + Clamp(see_value, -500, 500);
            return score;
        }
        default: {
            break;
        }
    }

    // 4) Only simple moves (non-captures and non-promotions) are handled below
    if (!IsSimpleFlag(flag)) {
        // Fallback priority for potential new flags in the future
        return 100000;
    }

    // 5) Cutoff moves (former killers) — two from|to hints
    if (SameKey(ctx.cutoff1, move)) {
        return 300000;
    }

    if (SameKey(ctx.cutoff2, move)) {
        return 290000;
    }

    // 6) History heuristic for simple moves
    int history_score = 0;
    if (ctx.history != nullptr) {
        int side_index = static_cast<int>(ctx.side_to_move);
        int from = static_cast<int>(move.GetFrom());
        int to   = static_cast<int>(move.GetTo());
        int hval = (*ctx.history)[side_index][from][to];

        if (hval < 0) {
            hval = 0;
        }

        if (hval > 16384) {
            hval = 16384;
        }

        history_score = 100000 + hval;
    }
    else {
        history_score = 100000;
    }

    return history_score;
}
