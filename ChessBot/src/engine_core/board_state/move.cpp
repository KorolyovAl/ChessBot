#include "move.h"

Move::Move(uint8_t from, uint8_t to,
           uint8_t attacker_type, uint8_t attacker_side,
           uint8_t defender_type, uint8_t defender_side,
           Flag flag)
    : from_(from), to_(to),
    attacker_type_(attacker_type),
    attacker_side_(attacker_side),
    defender_type_(defender_type),
    defender_side_(defender_side),
    flag_(flag)
{}

uint8_t Move::GetFrom() const {
    return from_;
}

uint8_t Move::GetTo() const {
    return to_;
}

uint8_t Move::GetAttackerType() const {
    return attacker_type_;
}

uint8_t Move::GetAttackerSide() const {
    return attacker_side_;
}

uint8_t Move::GetDefenderType() const {
    return defender_type_;
}

uint8_t Move::GetDefenderSide() const {
    return defender_side_;
}

Move::Flag Move::GetFlag() const {
    return flag_;
}

void Move::SetFrom(uint8_t value) {
    from_ = value;
}

void Move::SetTo(uint8_t value) {
    to_ = value;
}

void Move::SetAttackerType(uint8_t value) {
    attacker_type_ = value;
}

void Move::SetAttackerSide(uint8_t value) {
    attacker_side_ = value;
}

void Move::SetDefenderType(uint8_t value) {
    defender_type_ = value;
}

void Move::SetDefenderSide(uint8_t value) {
    defender_side_ = value;
}

void Move::SetFlag(Flag value) {
    flag_ = value;
}
