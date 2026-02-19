/****************************
* sliders_masks.h
*
* Compile‑time ray masks for rook, bishop and queen.
* kMasks[square][dir]  — bitboard of all squares “seen” in that direction
*                        until board edge (no occupancy considered).
*
****************************/

#pragma once

#include "../board_state/bitboard.h"
#include <array>

namespace SlidersMasks {

enum Direction : std::uint8_t {
    North       = 0,
    South       = 1,
    West        = 2,
    East        = 3,
    NorthWest   = 4,
    NorthEast   = 5,
    SouthWest   = 6,
    SouthEast   = 7,
    Count       = 8
};

// Build one ray mask starting from square p towards direction d
constexpr Bitboard GenerateRay(int p, Direction d) {
    Bitboard mask = 0;

    int x = p % 8;
    int y = p / 8;

    /* Determine step once */
    int dx = 0, dy = 0;
    switch (d) {
        case North:      dy =  1; break;
        case South:      dy = -1; break;
        case West:       dx = -1; break;
        case East:       dx =  1; break;
        case NorthWest:  dx = -1; dy =  1; break;
        case NorthEast:  dx =  1; dy =  1; break;
        case SouthWest:  dx = -1; dy = -1; break;
        case SouthEast:  dx =  1; dy = -1; break;
        default: break;
    }

    /* Walk until we fall off the board */
    while (true) {
        x += dx;
        y += dy;

        if (x < 0 || x > 7 || y < 0 || y > 7) break;

        int target = y * 8 + x;         // linear index 0..63
        mask |= (1ULL << target);       // set bit
    }
    return mask;                        // final ray mask
}

constexpr std::array<std::array<Bitboard, Direction::Count>, 64> GenerateAll() {
    std::array<std::array<Bitboard, Direction::Count>, 64> res{};

    for (int sq = 0; sq < 64; ++sq)
        for (int dir = 0; dir < Direction::Count; ++dir)
            res[sq][dir] = GenerateRay(sq, static_cast<Direction>(dir));

    return res;
}

inline constexpr auto kMasks = GenerateAll();

} // namespace SlidersMasks
