#include "repetition_history.h"

void RepetitionHistory::AddPosition(ZobristHash hash) {
    hashes_.push_back(hash);
}

void RepetitionHistory::Clear() {
    hashes_.clear();
}

uint8_t RepetitionHistory::GetRepetitionNumber(ZobristHash hash) const {
    uint8_t count = 0;
    for (const auto& past_hash : hashes_) {
        if (hash == past_hash) {
            count += 1;
        }
    }
    return count;
}
