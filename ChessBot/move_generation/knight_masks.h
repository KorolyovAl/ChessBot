/************************
* knight_masks.h
*
* Pre‑computed bitboard masks for knight movement.
* Generated fully at compile time via constexpr.
************************/

#pragma once

#include "../board_state/bitboard.h"
#include <array>

namespace KnightMasks {

// Build mask for one square (x,y).
// A knight has 8 possible L‑jumps (±1,±2) / (±2,±1).
constexpr Bitboard GenerateForSquare(int x, int y) {
    constexpr int dx[8] = { 1,  2,  2,  1, -1, -2, -2, -1 };
    constexpr int dy[8] = {-2, -1,  1,  2,  2,  1, -1, -2 };

    Bitboard mask = 0;                 // empty accumulator

    /* knight‑version: iterate 8 (dx,dy) L‑jumps */
    for (int i = 0; i < 8; ++i) {
        int tx = x + dx[i];            // target‑file
        int ty = y + dy[i];            // target‑rank

        // board bounds 0..7
        if (tx >= 0 && tx < 8 && ty >= 0 && ty < 8) {
            int target = ty * 8 + tx;  // linear 0..63
            mask |= (1ULL << target);  // set bit
        }
    }
    return mask;                       // ready mask for (x,y)
}

// Generate all 64 masks at compile time
constexpr std::array<Bitboard, 64> GenerateAll() {
    std::array<Bitboard, 64> result{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            result[y * 8 + x] = GenerateForSquare(x, y);
    return result;
}

// Accessor for static constexpr table
inline constexpr auto kMasks = GenerateAll();

} // namespace KnightMasks
