#include "static_exchange_evaluation.h"

#include <array>

#include "piece_values.h"
#include "../move_generation/pawn_attack_masks.h"
#include "../move_generation/knight_masks.h"
#include "../move_generation/king_masks.h"
#include "../move_generation/sliders_masks.h"
#include "../board_state/bitboard.h"

namespace {

// Centipawn value by piece type; king is treated as very large to avoid king trades
inline int PieceValueCp(int piece_type_index) {
    return EvalValues::kPieceValueCp[piece_type_index];
}

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

// Remove a single piece from the snapshot
inline void RemovePiece(BoardSnapshot& snapshot, Side side, int piece_type, uint8_t square) {
    snapshot.piece_bb[static_cast<int>(side)][piece_type] =
        BOp::Set_0(snapshot.piece_bb[static_cast<int>(side)][piece_type], square);
    if (side == Side::White) {
        snapshot.occ_white = BOp::Set_0(snapshot.occ_white, square);
    } else {
        snapshot.occ_black = BOp::Set_0(snapshot.occ_black, square);
    }
    snapshot.occ_all = BOp::Set_0(snapshot.occ_all, square);
}

// Add a single piece to the snapshot
inline void AddPiece(BoardSnapshot& snapshot, Side side, int piece_type, uint8_t square) {
    snapshot.piece_bb[static_cast<int>(side)][piece_type] =
        BOp::Set_1(snapshot.piece_bb[static_cast<int>(side)][piece_type], square);
    if (side == Side::White) {
        snapshot.occ_white = BOp::Set_1(snapshot.occ_white, square);
    } else {
        snapshot.occ_black = BOp::Set_1(snapshot.occ_black, square);
    }
    snapshot.occ_all = BOp::Set_1(snapshot.occ_all, square);
}

// Find which side occupies square (if any)
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

// Find piece type at square or -1 if empty
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

// Attackers to a target square, grouped by piece type for each side
struct AttackersByType {
    Bitboard pawns[2];
    Bitboard knights[2];
    Bitboard bishops[2];
    Bitboard rooks[2];
    Bitboard queens[2];
    Bitboard kings[2];
};

// First blocker along a ray from target; 64 if none
inline uint8_t NearestBlockerInRay(Bitboard ray_mask, const BoardSnapshot& snapshot, bool reverse) {
    Bitboard blockers = ray_mask & snapshot.occ_all;
    if (!blockers) {
        return 64;
    }
    if (reverse) {
        return BOp::BitScanReverse(blockers);
    } else {
        return BOp::BitScanForward(blockers);
    }
}

// If the piece at sq on side_index is bishop or queen, mark as diagonal attacker
inline void MaybeTagBishopOrQueen(const BoardSnapshot& snapshot, int side_index, uint8_t sq, AttackersByType& out) {
    if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Bishop], sq)) {
        out.bishops[side_index] = BOp::Set_1(out.bishops[side_index], sq);
    } else if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Queen], sq)) {
        out.queens[side_index] = BOp::Set_1(out.queens[side_index], sq);
    }
}

// If the piece at sq on side_index is rook or queen, mark as orthogonal attacker
inline void MaybeTagRookOrQueen(const BoardSnapshot& snapshot, int side_index, uint8_t sq, AttackersByType& out) {
    if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Rook], sq)) {
        out.rooks[side_index] = BOp::Set_1(out.rooks[side_index], sq);
    } else if (BOp::GetBit(snapshot.piece_bb[side_index][PieceType::Queen], sq)) {
        out.queens[side_index] = BOp::Set_1(out.queens[side_index], sq);
    }
}

// Forward declaration for LegalKingCaptureFilter
inline AttackersByType CollectAttackers(const BoardSnapshot& snapshot, uint8_t target_square);

// Remove illegal king captures from attacker sets
inline void LegalKingCaptureFilter(const BoardSnapshot& snapshot, uint8_t target_square, AttackersByType& attackers) {
    Side occ_side;
    int  occ_pt = PieceType::None;
    bool occupied = GetSideAt(snapshot, target_square, occ_side);
    if (occupied) {
        occ_pt = GetPieceTypeAt(snapshot, target_square);
    }

    for (int side_index = 0; side_index < 2; ++side_index) {
        Bitboard kings = attackers.kings[side_index];
        while (kings) {
            uint8_t from_sq = BOp::BitScanForward(kings);
            kings = BOp::Set_0(kings, from_sq);

            // If target is occupied by own piece, capture is impossible
            if (occupied && occ_side == static_cast<Side>(side_index)) {
                attackers.kings[side_index] = BOp::Set_0(attackers.kings[side_index], from_sq);
                continue;
            }

            // Simulate Kx on a copy of the snapshot
            BoardSnapshot tmp = snapshot;
            if (occupied && occ_side != static_cast<Side>(side_index)) {
                RemovePiece(tmp, occ_side, occ_pt, target_square);
            }
            RemovePiece(tmp, static_cast<Side>(side_index), PieceType::King, from_sq);
            AddPiece(tmp, static_cast<Side>(side_index), PieceType::King, target_square);

            // Check if the king’s new square is attacked by the opponent
            AttackersByType enemy_atks = CollectAttackers(tmp, target_square);
            const int enemy = 1 - side_index;
            Bitboard any_enemy =
                enemy_atks.pawns [enemy]
                | enemy_atks.knights[enemy]
                | enemy_atks.bishops[enemy]
                | enemy_atks.rooks [enemy]
                | enemy_atks.queens[enemy]
                | enemy_atks.kings [enemy];

            if (any_enemy) {
                // King after capture is under attack, so this capture is illegal
                attackers.kings[side_index] = BOp::Set_0(attackers.kings[side_index], from_sq);
            }
        }
    }
}

// Build attacker sets for the current snapshot at target_square
inline AttackersByType CollectAttackers(const BoardSnapshot& snapshot, uint8_t target_square) {
    AttackersByType attackers{};
    Bitboard mask_knight = KnightMasks::kMasks[target_square];
    Bitboard mask_king   = KingMasks::kMasks[target_square];

    attackers.knights[0] = snapshot.piece_bb[0][PieceType::Knight] & mask_knight;
    attackers.knights[1] = snapshot.piece_bb[1][PieceType::Knight] & mask_knight;
    attackers.kings[0]   = snapshot.piece_bb[0][PieceType::King]   & mask_king;
    attackers.kings[1]   = snapshot.piece_bb[1][PieceType::King]   & mask_king;

    attackers.pawns[0] = snapshot.piece_bb[0][PieceType::Pawn] &
                         PawnMasks::kAttack[static_cast<int>(Side::Black)][target_square];
    attackers.pawns[1] = snapshot.piece_bb[1][PieceType::Pawn] &
                         PawnMasks::kAttack[static_cast<int>(Side::White)][target_square];

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

    LegalKingCaptureFilter(snapshot, target_square, attackers);

    return attackers;
}

// Pick least valuable attacker for side, return false if no attackers remain
inline bool ExtractLeastValuable(const AttackersByType& attackers, Side side,
                                 uint8_t& from_square, int& attacker_piece_type) {
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

// Helpers for capture plus promotion handling in the first halfmove of SEE
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

// Convert promotion flag to promoted piece type or -1 if not a promotion
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

enum class SeeMode { On, Capture };

// Common part of SEE for a given target square
static inline int SeeSquare(BoardSnapshot& s, uint8_t target_square, Side occ_side, int occ_pt,
                            Side side_to_move, SeeMode mode) {
    int gains_cp[32];
    int depth = -1;

    while (true) {
        AttackersByType atks = CollectAttackers(s, target_square);

        uint8_t from = 64;
        int atk_pt = -1;
        bool has = ExtractLeastValuable(atks, side_to_move, from, atk_pt);
        if (!has) {
            break;
        }

        RemovePiece(s, occ_side, occ_pt, target_square);
        RemovePiece(s, side_to_move, atk_pt, from);
        AddPiece   (s, side_to_move, atk_pt, target_square);

        ++depth;
        if (depth == 0) {
            gains_cp[depth] = PieceValueCp(occ_pt);
        } else {
            gains_cp[depth] = gains_cp[depth - 1] - PieceValueCp(occ_pt);
        }

        occ_side = side_to_move;
        occ_pt   = atk_pt;
        side_to_move = (side_to_move == Side::White ? Side::Black : Side::White);
    }

    if (depth < 0) {
        return 0;
    }

    if (mode == SeeMode::On) {
        // Collapse only the tail (i >= 1) to avoid stand-pat on the root
        for (int i = depth - 1; i >= 1; --i) {
            gains_cp[i] = -std::max(-gains_cp[i], gains_cp[i + 1]);
        }

        // If there was at least one capture, return the value after the first move
        // (the opponent has already chosen the best reply)
        if (depth >= 1) {
            return gains_cp[1];
        } else {
            // Single capture with no reply
            return gains_cp[0];
        }
    } else {
        // Full collapse: final result of the given capture sequence
        for (int i = depth - 1; i >= 0; --i) {
            gains_cp[i] = -std::max(-gains_cp[i], gains_cp[i + 1]);
        }
        return gains_cp[0];
    }
}
} // namespace

int StaticExchangeEvaluation::On(const Pieces& pieces, uint8_t target_square, Side owner_square) {
    BoardSnapshot s = MakeSnapshot(pieces);

    if (!BOp::GetBit(s.occ_all, target_square)) {
        return 0;
    }

    Side occ_side;
    if (!GetSideAt(s, target_square, occ_side)) {
        return 0;
    }

    int occ_pt = GetPieceTypeAt(s, target_square);
    Side side_to_move = Pieces::Inverse(owner_square);

    return SeeSquare(s, target_square, occ_side, occ_pt, side_to_move, SeeMode::On);
}

int StaticExchangeEvaluation::Capture(const Pieces& pieces, const Move& move) {
    BoardSnapshot snapshot = MakeSnapshot(pieces);

    const Move::Flag flag = move.GetFlag();

    // Attacking side and piece type before promotion
    const Side attacker_side = static_cast<Side>(move.GetAttackerSide());
    int attacker_type = static_cast<int>(move.GetAttackerType());

    const uint8_t to_square = move.GetTo();
    const uint8_t from_square = move.GetFrom();

    // Victim square and type: normal capture or en passant (pawn behind to_square)
    uint8_t victim_square = to_square;
    Side victim_side = (attacker_side == Side::White ? Side::Black : Side::White);
    int victim_type = -1;

    if (flag == Move::Flag::EnPassantCapture) {
        victim_square = (attacker_side == Side::White)
        ? static_cast<uint8_t>(to_square - 8)
        : static_cast<uint8_t>(to_square + 8);
        victim_type = PieceType::Pawn;

        // Guard against inconsistent position: required pawn is missing
        if (!BOp::GetBit((attacker_side == Side::White ? snapshot.occ_black : snapshot.occ_white),
                         victim_square)) {
            return 0;
        }
    } else {
        victim_type = GetPieceTypeAt(snapshot, to_square);

        // No piece on target square, capture is invalid for SEE
        if (victim_type < 0) {
            return 0;
        }
    }

    // Remove victim from the snapshot
    RemovePiece(snapshot, victim_side, victim_type, victim_square);

    // Handle promotion: after the first move the promoted piece occupies the square
    if (IsPromotion(flag)) {
        const int promo_type = PromotionTypeFromFlag(flag);
        if (promo_type >= 0) {
            // Remove attacker of original type
            RemovePiece(snapshot, attacker_side, attacker_type, from_square);
            // Place promoted piece on target square
            AddPiece(snapshot, attacker_side, promo_type, to_square);

            // Tail starts from the square occupied by the promoted piece,
            // with the opponent to move
            const Side stm_after = (attacker_side == Side::White ? Side::Black : Side::White);
            const int tail =
                SeeSquare(snapshot, to_square, attacker_side, promo_type, stm_after, SeeMode::Capture);
            return PieceValueCp(victim_type) - tail;
        }
        // If promotion flag is inconsistent, treat it as a normal capture without promotion
    }

    // Normal capture: move attacker to target square
    RemovePiece(snapshot, attacker_side, attacker_type, from_square);
    AddPiece (snapshot, attacker_side, attacker_type, to_square);

    // After the first move the attacker occupies the square and the opponent moves
    const Side stm_after = (attacker_side == Side::White ? Side::Black : Side::White);

    const int tail =
        SeeSquare(snapshot, to_square, attacker_side, attacker_type, stm_after, SeeMode::Capture);

    return PieceValueCp(victim_type) - tail;
}
