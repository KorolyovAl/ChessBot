/*******************************
* pawn_attack_masks.h
*
* Pre‑computed **attack** masks for pawns.
* Only diagonal capture squares are stored; forward pushes are
* generated on‑the‑fly with simple bit‑shifts.
*
* Usage:
*   Bitboard wAtt = PawnMasks::kAttack[static_cast<int>(Side::White)][square];
*   if (wAtt & blackPieces) { … }
*******************************/

#pragma once

#include "../board_state/bitboard.h"
#include "../board_state/pieces.h"          // for enum class Side
#include <array>

namespace PawnMasks {

    /*-----------------------------------------------------------
          Build diagonal capture mask for one square & side
            White: NE (x+1,y+1)  NW (x-1,y+1)
            Black: SE (x+1,y-1)  SW (x-1,y-1)
    -----------------------------------------------------------*/
    constexpr Bitboard GenerateAttack(int sq, Side side) {
        int x = sq % 8;
        int y = sq / 8;

        Bitboard mask = 0;                   // empty accumulator

        auto add = [&](int nx, int ny) {
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int target = ny * 8 + nx;
                mask |= (1ULL << target);   // set bit
            }
        };

        if (side == Side::White) {
            add(x - 1, y + 1);   // NW
            add(x + 1, y + 1);   // NE
        } else {                 // Side::Black
            add(x - 1, y - 1);   // SW
            add(x + 1, y - 1);   // SE
        }
        return mask;             // ready attack mask
    }


    // Build full table 2 × 64  (side, square)  at compile‑time
    constexpr std::array<std::array<Bitboard, 64>, 2> GenerateAll() {
        std::array<std::array<Bitboard, 64>, 2> tbl{};

        for (int s = 0; s < 2; ++s)
            for (int sq = 0; sq < 64; ++sq)
                tbl[s][sq] = GenerateAttack(sq, static_cast<Side>(s));

        return tbl;
    }

    inline constexpr auto kAttack = GenerateAll();

} // namespace PawnMasks
