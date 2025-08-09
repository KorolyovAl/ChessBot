/************************************************
* RepetitionHistory — tracks the history of Zobrist hashes
* to detect position repetition for draw conditions.
*
* Used by game logic to determine how many times
* a position has occurred. Supports reset.
*
* Contains:
* - AddPosition() for logging hashes
* - GetRepetitionNumber() to count repeats
* - Clear() to reset the history
************************************************/

#pragma once

#include <vector>
#include "zobrist_hash.h"

class RepetitionHistory {
public:
    RepetitionHistory() = default;

    void AddPosition(ZobristHash hash);
    void Clear();

    uint8_t GetRepetitionNumber(ZobristHash hash) const;

private:
    std::vector<ZobristHash> hashes_;
};
