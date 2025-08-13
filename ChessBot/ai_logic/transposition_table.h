/************
* TranspositionTable — fixed-size hash table over Zobrist keys.
* Stores depth, score, bound type, and best move for move ordering.
************/
#pragma once

#include <cstdint>
#include <vector>

#include "../board_state/move.h"

class TranspositionTable {
public:
    enum class Bound : uint8_t { Exact, Lower, Upper };

    struct Entry {
        uint64_t key = 0;
        int16_t  score = 0;               // centipawns (with mate encoding)
        int16_t  best_from = -1;          // compact store
        int16_t  best_to   = -1;
        uint8_t  best_attacker_type = 255;
        uint8_t  best_attacker_side = 255;
        uint8_t  best_flag = 0;
        int8_t   depth = -1;              // ply
        Bound    bound = Bound::Exact;
    };

    explicit TranspositionTable(std::size_t hash_size_mb = 64);

    void Clear();

    // Returns true if entry is usable for the (depth, alpha, beta) window.
    bool Probe(uint64_t key, int depth, int alpha, int beta, int& out_score, Move& out_best_move) const;

    void Store(uint64_t key, int depth, int score, Bound bound, const Move& best_move);

private:
    std::vector<Entry> table_;
    uint64_t index_mask_ = 0;

    static Move EntryToMove(const Entry& entry);
    static Entry MoveToEntry(uint64_t key, int depth, int score, Bound bound, const Move& move);
};
