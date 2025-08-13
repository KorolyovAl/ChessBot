#include "move_ordering.h"
#include "../board_state/bitboard.h"

namespace {
    constexpr int VICTIM_VALUE[] = { 100, 320, 330, 500, 900, 10000 }; // K is huge
    constexpr int ATTACKER_PENALTY[] = { 100, 320, 330, 500, 900, 10 }; // K small

    inline int GetVictimTypeIndex(const Pieces& pieces,
                                  Side victim_side,
                                  uint8_t target_square) {
        for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
            const PieceType piece_type = static_cast<PieceType>(type_index);
            const Bitboard bb = pieces.GetPieceBitboard(victim_side, piece_type);
            if (BOp::GetBit(bb, target_square)) {
                return type_index;
            }
        }
        return -1;
    }
}

int MoveOrdering::Score(const Move& move, const Pieces& pieces) {
    // Promotions first
    switch (move.GetFlag()) {
        case Move::Flag::PromoteToQueen: {
            return 9000;
        }
        case Move::Flag::PromoteToRook: {
            return 8000;
        }
        case Move::Flag::PromoteToBishop:
        case Move::Flag::PromoteToKnight: {
            return 7000;
        }
        case Move::Flag::EnPassantCapture: {
            return 6000;
        }
        case Move::Flag::Capture: {
            const Side victim_side = (move.GetAttackerSide() == static_cast<uint8_t>(Side::White))
            ? Side::Black
            : Side::White;
            const int victim_index = GetVictimTypeIndex(pieces, victim_side, move.GetTo());
            const int attacker_index = static_cast<int>(move.GetAttackerType());

            if (victim_index >= 0) {
                return 5000 + VICTIM_VALUE[victim_index] - ATTACKER_PENALTY[attacker_index];
            } else {
                // Fallback if board does not expose victim (should be rare)
                return 5000;
            }
        }
        default: {
            break;
        }
    }

    // Quiet moves
    return 0;
}
