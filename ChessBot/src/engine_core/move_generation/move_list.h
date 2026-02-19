/***********************
* MoveList
*
* Fixed-size container for storing generated moves.
* Supports direct access, iteration, and safe insertion.
* Maximum capacity is 218 moves (upper-bound for legal chess move count).
***********************/

#pragma once

#include <array>
#include "../board_state/move.h"

class MoveList {
public:
    MoveList();

    // Returns reference to move at given index
    Move& operator[](uint8_t index);

    // Returns a copy of the move at given index
    Move operator[](uint8_t index) const;

    // Adds a move to the list (if capacity allows)
    void Push(const Move& move);

    // Returns the current number of moves
    uint8_t GetSize() const;

    // Iterators for range-based for
    auto begin() {
        return moves_.begin();
    }
    auto end()   {
        return moves_.begin() + size_;
    }

    auto begin() const {
        return moves_.begin();
    }
    auto end()   const {
        return moves_.begin() + size_;
    }

private:
    std::array<Move, 218> moves_{};  // Maximum legal move count in chess
    std::uint8_t size_ = 0;
};
