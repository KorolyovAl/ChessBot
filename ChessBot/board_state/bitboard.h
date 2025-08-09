/*********************************************
 * Bitboard — representation of a chessboard.
 *
 * Each square on the 8x8 board corresponds to a bit
 * in a 64-bit unsigned integer. Index 0 = A1, 63 = H8.
 *
 * This system is used for storing piece positions,
 * move masks, attack maps, etc.
 *
 * Contains:
 * - Bitboard type alias (uint64_t)
 * - Basic operations: set, clear, check, popcount, bsf, bsr
 * - Precomputed row and column masks
 *********************************************/

#pragma once

#include <array>
#include <cstdint>

#if __has_include(<bit>)
#include <bit>
#endif

using Bitboard = std::uint64_t;

// BitScan lookup table (defined in Bitboard.cpp)
extern std::array<uint8_t, 64> BitScanTable;

// Bitboard operations
namespace BOp {

// Set bit at given square
inline Bitboard Set_1(Bitboard bb, uint8_t square) {
    return bb | (1ull << square);
}

// Clear bit at given square
inline Bitboard Set_0(Bitboard bb, uint8_t square) {
    return bb & ~(1ull << square);
}

// Check if bit is set
inline bool GetBit(Bitboard bb, uint8_t square) {
    return (bb & (1ull << square)) != 0;
}

// Count number of set bits (popcount)
inline uint8_t Count_1(Bitboard bb) {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
    return static_cast<uint8_t>(std::popcount(bb));
#else
    bb -= (bb >> 1) & 0x5555555555555555llu;
    bb = ((bb >> 2) & 0x3333333333333333llu) + (bb & 0x3333333333333333llu);
    bb = ((((bb >> 4) + bb) & 0x0F0F0F0F0F0F0F0Fllu)
          * 0x0101010101010101) >> 56;
    return bb;
#endif
}

// Bit Scan Forward — index of least significant 1-bit
inline uint8_t BitScanForward(Bitboard bb) {
    return BitScanTable[((bb ^ (bb - 1)) * 0x03f79d71b4cb0a89) >> 58];
}

// Bit Scan Reverse — index of most significant 1-bit
inline uint8_t BitScanReverse(Bitboard bb) {
    bb |= (bb >> 1);
    bb |= (bb >> 2);
    bb |= (bb >> 4);
    bb |= (bb >> 8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);

    return BitScanTable[(bb * 0x03f79d71b4cb0a89) >> 58];
}
}

// Precomputed bit masks for ranks (rows)
namespace BRows {

inline std::array<Bitboard, 8> CalcRows() {
    std::array<Bitboard, 8> rows{};
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            rows[y] = BOp::Set_1(rows[y], y * 8 + x);
        }
    }
    return rows;
}

inline std::array<Bitboard, 8> Rows = CalcRows();

inline std::array<Bitboard, 8> CalcInversionRows() {
    std::array<Bitboard, 8> inv{};
    for (uint8_t i = 0; i < 8; i++) inv[i] = ~Rows[i];
    return inv;
}

inline std::array<Bitboard, 8> InversionRows = CalcInversionRows();
}

// Precomputed bit masks for files (columns)
namespace BColumns {

inline std::array<Bitboard, 8> CalcColumns() {
    std::array<Bitboard, 8> columns{};
    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 0; y < 8; y++) {
            columns[x] = BOp::Set_1(columns[x], y * 8 + x);
        }
    }
    return columns;
}

inline std::array<Bitboard, 8> Columns = CalcColumns();

inline std::array<Bitboard, 8> CalcInversionColumns() {
    std::array<Bitboard, 8> inv{};
    for (uint8_t i = 0; i < 8; i++) inv[i] = ~Columns[i];
    return inv;
}

inline std::array<Bitboard, 8> InversionColumns = CalcInversionColumns();
};
