#include "static_exchange_evaluation.h"

#include <array>

#include "piece_values.h"
#include "../move_generation/pawn_attack_masks.h"
#include "../move_generation/knight_masks.h"
#include "../move_generation/king_masks.h"
#include "../move_generation/sliders_masks.h"
#include "../board_state/bitboard.h"

namespace {

    // Centipawn value by piece type; treat king as very large to avoid king trades.
    inline int PieceValueCp(int piece_type_index) {
        if (piece_type_index == PieceType::King) {
            return 10'000;
        }
        return EvalValues::kPieceValueCp[piece_type_index];
    }

    // Local snapshot of bitboards for hypothetical exchanges.
    struct BoardSnapshot {
        std::array<std::array<Bitboard, PieceType::Count>, 2> piece_bb; // [side][piece]
        Bitboard occ_white;
        Bitboard occ_black;
        Bitboard occ_all;
    };

    // Build a snapshot from current Pieces.
    inline BoardSnapshot MakeSnapshot(const Pieces& pieces) {
        BoardSnapshot snapshot;
        snapshot.piece_bb  = pieces.GetPieceBitboards();
        snapshot.occ_white = pieces.GetSideBoard(Side::White);
        snapshot.occ_black = pieces.GetSideBoard(Side::Black);
        snapshot.occ_all   = pieces.GetAllBitboard();
        return snapshot;
    }

    // Remove a single piece from the snapshot.
    inline void RemovePiece(BoardSnapshot& snapshot, Side side, int piece_type, uint8_t square) {
        snapshot.piece_bb[static_cast<int>(side)][piece_type] = BOp::Set_0(snapshot.piece_bb[static_cast<int>(side)][piece_type], square);
        if (side == Side::White) {
            snapshot.occ_white = BOp::Set_0(snapshot.occ_white, square);
        }
        else {
            snapshot.occ_black = BOp::Set_0(snapshot.occ_black, square);
        }
        snapshot.occ_all = BOp::Set_0(snapshot.occ_all, square);
    }

    // Find which side occupies `square` (if any).
    inline bool GetSideAt(const BoardSnapshot& snapshot, uint8_t square, Side& side_out) {
        if (BOp::GetBit(snapshot.occ_white, square)) {
            side_out = Side::White;
            return true;
        }
        if (BOp::GetBit(snapshot.occ_black, square)) {
            side_out = Side::Black;
            return true;
        }
        return false;
    }

    // Find piece type at `square` (or -1 if empty).
    inline int GetPieceTypeAt(const BoardSnapshot& snapshot, uint8_t square) {
        for (int piece_type_index = 0; piece_type_index < PieceType::Count; ++piece_type_index) {
            if (BOp::GetBit(snapshot.piece_bb[static_cast<int>(Side::White)][piece_type_index], square)) {
                return piece_type_index;
            }
            if (BOp::GetBit(snapshot.piece_bb[static_cast<int>(Side::Black)][piece_type_index], square)) {
                return piece_type_index;
            }
        }
        return -1;
    }

    // Attackers to a target square, grouped by piece type for each side.
    struct AttackersByType {
        Bitboard pawns[2];
        Bitboard knights[2];
        Bitboard bishops[2];
        Bitboard rooks[2];
        Bitboard queens[2];
        Bitboard kings[2];
    };

    // First blocker along a ray from target; 64 if none.
    inline uint8_t NearestBlockerInRay(Bitboard ray_mask, const BoardSnapshot& snapshot, bool reverse) {
        Bitboard blockers = ray_mask & snapshot.occ_all;
        if (!blockers) {
            return 64;
        }
        if (reverse) {
            return BOp::BitScanReverse(blockers);
        }
        else {
            return BOp::BitScanForward(blockers);
        }
    }

    // If the piece at `sq` on `side_index` is bishop/queen, mark as diagonal attacker.
    inline void MaybeTagBishopOrQueen(const BoardSnapshot& snapshot, int side_index, uint8_t sq, AttackersByType& out) {
        if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Bishop], sq)) {
            out.bishops[side_index] = BOp::Set_1(out.bishops[side_index], sq);
        }
        else if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Queen], sq)) {
            out.queens[side_index] = BOp::Set_1(out.queens[side_index], sq);
        }
    }

    // If the piece at `sq` on `side_index` is rook/queen, mark as orthogonal attacker.
    inline void MaybeTagRookOrQueen(const BoardSnapshot& snapshot, int side_index, uint8_t sq, AttackersByType& out) {
        if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Rook], sq)) {
            out.rooks[side_index] = BOp::Set_1(out.rooks[side_index], sq);
        } else if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Queen], sq)) {
            out.queens[side_index] = BOp::Set_1(out.queens[side_index], sq);
        }
    }

    // Build attacker sets for the current snapshot at `target_square`.
    inline AttackersByType CollectAttackers(const BoardSnapshot& snapshot, uint8_t target_square) {
        AttackersByType attackers{};
        Bitboard mask_knight = KnightMasks::kMasks[target_square];
        Bitboard mask_king   = KingMasks::kMasks[target_square];

        attackers.knights[0] = snapshot.piece_bb[0][PieceType::Knight] & mask_knight;
        attackers.knights[1] = snapshot.piece_bb[1][PieceType::Knight] & mask_knight;
        attackers.kings[0]   = snapshot.piece_bb[0][PieceType::King]   & mask_king;
        attackers.kings[1]   = snapshot.piece_bb[1][PieceType::King]   & mask_king;

        attackers.pawns[0] = snapshot.piece_bb[0][PieceType::Pawn] & PawnMasks::kAttack[static_cast<int>(Side::Black)][target_square];
        attackers.pawns[1] = snapshot.piece_bb[1][PieceType::Pawn] & PawnMasks::kAttack[static_cast<int>(Side::White)][target_square];

        using D = SlidersMasks::Direction;
        uint8_t blocker_square = 64; // first blocker on ray, if any

        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::NorthWest], snapshot, false);
        if (blocker_square < 64) {
            MaybeTagBishopOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagBishopOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::NorthEast], snapshot, false);
        if (blocker_square < 64) {
            MaybeTagBishopOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagBishopOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::SouthWest], snapshot, true);
        if (blocker_square < 64) {
            MaybeTagBishopOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagBishopOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::SouthEast], snapshot, true);
        if (blocker_square < 64) {
            MaybeTagBishopOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagBishopOrQueen(snapshot, 1, blocker_square, attackers);
        }

        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::North], snapshot, false);
        if (blocker_square < 64) {
            MaybeTagRookOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagRookOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::South], snapshot, true);
        if (blocker_square < 64) {
            MaybeTagRookOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagRookOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::West], snapshot, true);
        if (blocker_square < 64) {
            MaybeTagRookOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagRookOrQueen(snapshot, 1, blocker_square, attackers);
        }
        blocker_square = NearestBlockerInRay(SlidersMasks::kMasks[target_square][D::East], snapshot, false);
        if (blocker_square < 64) {
            MaybeTagRookOrQueen(snapshot, 0, blocker_square, attackers);
            MaybeTagRookOrQueen(snapshot, 1, blocker_square, attackers);
        }

        return attackers;
    }

    // Pick least-valuable attacker for `side`; return false if no attackers remain.
    inline bool ExtractLeastValuable(const AttackersByType& attackers, Side side, uint8_t& from_square, int& attacker_piece_type) {
        int s = static_cast<int>(side);

        if (attackers.pawns[s]) {
            from_square = BOp::BitScanForward(attackers.pawns[s]);
            attacker_piece_type = PieceType::Pawn;
            return true;
        }
        if (attackers.knights[s]) {
            from_square = BOp::BitScanForward(attackers.knights[s]);
            attacker_piece_type = PieceType::Knight;
            return true;
        }
        if (attackers.bishops[s]) {
            from_square = BOp::BitScanForward(attackers.bishops[s]);
            attacker_piece_type = PieceType::Bishop;
            return true;
        }
        if (attackers.rooks[s]) {
            from_square = BOp::BitScanForward(attackers.rooks[s]);
            attacker_piece_type = PieceType::Rook;
            return true;
        }
        if (attackers.queens[s]) {
            from_square = BOp::BitScanForward(attackers.queens[s]);
            attacker_piece_type = PieceType::Queen;
            return true;
        }
        if (attackers.kings[s]) {
            from_square = BOp::BitScanForward(attackers.kings[s]);
            attacker_piece_type = PieceType::King;
            return true;
        }

        return false;
    }

    // Helpers for (capture+promotion) handling in the first halfmove of SEE.
    inline bool IsPromotion(Move::Flag flag) {
        if (flag == Move::Flag::PromoteToQueen) {
            return true;
        }
        if (flag == Move::Flag::PromoteToRook) {
            return true;
        }
        if (flag == Move::Flag::PromoteToBishop) {
            return true;
        }
        if (flag == Move::Flag::PromoteToKnight) {
            return true;
        }
        return false;
    }

    inline int PromotionTypeFromFlag(Move::Flag flag) {
        switch (flag) {
            case Move::Flag::PromoteToQueen: {
                return PieceType::Queen;
            }
            case Move::Flag::PromoteToRook: {
                return PieceType::Rook;
            }
            case Move::Flag::PromoteToBishop: {
                return PieceType::Bishop;
            }
            case Move::Flag::PromoteToKnight: {
                return PieceType::Knight;
            }
            default: {
                return -1;
            }
        }
    }
} // namespace

int StaticExchangeEvaluation::On(const Pieces& pieces, uint8_t target_square, Side side_to_move) {
    BoardSnapshot snapshot = MakeSnapshot(pieces);

    Side occupant_side;
    int occupant_piece_type = -1;

    if (BOp::GetBit(snapshot.occ_all, target_square)) {
        bool have_side = GetSideAt(snapshot, target_square, occupant_side);
        if (!have_side) {
            return 0;
        }
        occupant_piece_type = GetPieceTypeAt(snapshot, target_square);
    }
    else {
        return 0;
    }

    // gains[i] — net material after i-th capture (side alternates each halfmove)
    int gains_cp[32];
    int exchange_depth = 0;

    gains_cp[exchange_depth] = PieceValueCp(occupant_piece_type);

    // Assume the starting side captures the occupant.
    RemovePiece(snapshot, occupant_side, occupant_piece_type, target_square);

    Side side_to_move_current = side_to_move;

    // Alternating captures by least-valuable attacker until none remain.
    while (true) {
        AttackersByType attackers = CollectAttackers(snapshot, target_square);

        uint8_t from_square = 64;
        int attacker_piece_type = -1;
        bool has_attacker = ExtractLeastValuable(attackers, side_to_move_current, from_square, attacker_piece_type);
        if (!has_attacker) {
            break;
        }

        RemovePiece(snapshot, side_to_move_current, attacker_piece_type, from_square);

        exchange_depth = exchange_depth + 1;
        gains_cp[exchange_depth] = PieceValueCp(attacker_piece_type) - gains_cp[exchange_depth - 1];

        side_to_move_current = (side_to_move_current == Side::White ? Side::Black : Side::White);
    }

    // Choose best alternating outcome (fail-soft style).
    for (int i = exchange_depth - 1; i >= 0; --i) {
        int child = gains_cp[i + 1];
        int self  = gains_cp[i];
        int best  = (self > -child ? self : -child);
        gains_cp[i] = best;
    }

    return gains_cp[0];
}

int StaticExchangeEvaluation::Capture(const Pieces& pieces, const Move& move) {
    Move::Flag flag = move.GetFlag();

    BoardSnapshot snapshot = MakeSnapshot(pieces);

    Side attacker_side = static_cast<Side>(move.GetAttackerSide());
    int  attacker_type_raw = static_cast<int>(move.GetAttackerType());

    uint8_t to_square = move.GetTo();
    uint8_t victim_square = to_square;
    Side victim_side = (attacker_side == Side::White ? Side::Black : Side::White);
    int victim_type = -1;

    // En-passant: victim pawn sits behind the destination square.
    if (flag == Move::Flag::EnPassantCapture) {
        if (attacker_side == Side::White) {
            victim_square = static_cast<uint8_t>(to_square - 8);
        }
        else {
            victim_square = static_cast<uint8_t>(to_square + 8);
        }
        victim_type = PieceType::Pawn;
    }
    else {
        victim_type = GetPieceTypeAt(snapshot, victim_square);
        if (victim_type < 0) {
            return 0;
        }
    }

    int gains_cp[32];
    int exchange_depth = 0;

    gains_cp[exchange_depth] = PieceValueCp(victim_type);

    RemovePiece(snapshot, victim_side, victim_type, victim_square);

    uint8_t from_square = move.GetFrom();

    // If this capture promotes, the new piece value counts as the risk for the reply.
    int attacker_type_after = attacker_type_raw;
    if (IsPromotion(flag)) {
        int promo_type = PromotionTypeFromFlag(flag);
        if (promo_type >= 0) {
            attacker_type_after = promo_type;
        }
    }

    RemovePiece(snapshot, attacker_side, attacker_type_raw, from_square);

    exchange_depth = exchange_depth + 1;
    gains_cp[exchange_depth] = PieceValueCp(attacker_type_after) - gains_cp[exchange_depth - 1];

    Side side_to_move_current = (attacker_side == Side::White ? Side::Black : Side::White);

    while (true) {
        AttackersByType attackers = CollectAttackers(snapshot, to_square);

        uint8_t lva_from = 64;
        int lva_type = -1;
        bool has_attacker = ExtractLeastValuable(attackers, side_to_move_current, lva_from, lva_type);
        if (!has_attacker) {
            break;
        }

        RemovePiece(snapshot, side_to_move_current, lva_type, lva_from);

        exchange_depth = exchange_depth + 1;
        gains_cp[exchange_depth] = PieceValueCp(lva_type) - gains_cp[exchange_depth - 1];

        side_to_move_current = (side_to_move_current == Side::White ? Side::Black : Side::White);
    }

    for (int i = exchange_depth - 1; i >= 0; --i) {
        int child = gains_cp[i + 1];
        int self  = gains_cp[i];
        int best  = (self > -child ? self : -child);
        gains_cp[i] = best;
    }

    return gains_cp[0];
}
