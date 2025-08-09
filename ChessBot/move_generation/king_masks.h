/***********************
* king_masks.h
*
* Pre‑computed bitboard masks for king movement.
* All masks are generated at compile time via constexpr,
* so zero runtime cost on program start‑up.
***********************/

#pragma once

#include "../board_state/bitboard.h"
#include <array>

namespace KingMasks {

// Build mask for one square (x,y).
// A king can move to any of the 8 neighbouring squares.
constexpr Bitboard GenerateForSquare(int x, int y) {
    Bitboard mask = 0;                 // empty accumulator

    /* king‑version: iterate the 8 neighbouring squares */
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;   // skip origin

            int tx = x + dx;           // target‑file
            int ty = y + dy;           // target‑rank

            // board bounds 0..7
            if (tx >= 0 && tx < 8 && ty >= 0 && ty < 8) {
                int target = ty * 8 + tx;  // linear 0..63
                mask |= (1ULL << target);  // set bit
            }
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

} // namespace KingMasks
