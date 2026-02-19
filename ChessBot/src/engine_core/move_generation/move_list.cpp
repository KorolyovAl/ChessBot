#include "move_list.h"

MoveList::MoveList() : size_(0) {}

Move& MoveList::operator[](uint8_t index) {
    return moves_[index];
}

Move MoveList::operator[](uint8_t index) const {
    return moves_[index];
}

void MoveList::Push(const Move& move) {
    if (size_ < moves_.size()) {
        moves_[size_++] = move;
    }
    // else: silently ignore overflow (can log warning if needed)
}

uint8_t MoveList::GetSize() const {
    return size_;
}
