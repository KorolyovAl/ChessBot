#include "transposition_table.h"

static uint64_t NextPowerOfTwo(uint64_t value) {
    if (value == 0) {
        return 1;
    }

    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;

    return value + 1;
}

TranspositionTable::TranspositionTable(std::size_t hash_size_mb) {
    const uint64_t bytes = static_cast<uint64_t>(hash_size_mb) * 1024ull * 1024ull;
    uint64_t entry_count = bytes / sizeof(Entry);

    if (entry_count < 1) {
        entry_count = 1;
    }

    entry_count = NextPowerOfTwo(entry_count);

    table_.assign(entry_count, Entry{});
    index_mask_ = entry_count - 1;
}

void TranspositionTable::Clear() {
    for (auto& entry : table_) {
        entry = Entry{};
    }
}

bool TranspositionTable::Probe(uint64_t key, int depth, int alpha, int beta, int& out_score, Move& out_best_move) const {
    const Entry& entry = table_[key & index_mask_];

    if (entry.key != key) {
        return false;
    }

    out_best_move = EntryToMove(entry);

    if (entry.depth >= depth) {
        out_score = entry.score;

        if (entry.bound == Bound::Exact) {
            return true;
        }
        if (entry.bound == Bound::Lower && out_score >= beta) {
            return true;
        }
        if (entry.bound == Bound::Upper && out_score <= alpha) {
            return true;
        }
    }

    return false;
}

void TranspositionTable::Store(uint64_t key, int depth, int score, Bound bound, const Move& best_move) {
    table_[key & index_mask_] = MoveToEntry(key, depth, score, bound, best_move);
}

Move TranspositionTable::EntryToMove(const Entry& entry) {
    Move move;

    if (entry.best_from >= 0) {
        move.SetFrom(static_cast<uint8_t>(entry.best_from));
        move.SetTo(static_cast<uint8_t>(entry.best_to));
        move.SetAttackerType(entry.best_attacker_type);
        move.SetAttackerSide(entry.best_attacker_side);
        move.SetFlag(static_cast<Move::Flag>(entry.best_flag));
    }

    return move;
}

TranspositionTable::Entry TranspositionTable::MoveToEntry(uint64_t key, int depth, int score, Bound bound, const Move& move) {
    Entry entry{};

    entry.key = key;
    entry.depth = static_cast<int8_t>(depth);
    entry.score = static_cast<int16_t>(score);
    entry.bound = bound;

    entry.best_from = move.GetFrom();
    entry.best_to = move.GetTo();
    entry.best_attacker_type = move.GetAttackerType();
    entry.best_attacker_side = move.GetAttackerSide();
    entry.best_flag = static_cast<uint8_t>(move.GetFlag());

    return entry;
}
