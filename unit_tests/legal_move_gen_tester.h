#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <string>
#include <cstdint>
#include <iostream>

#include "../ChessBot/src/engine_core/board_state/position.h"
#include "../ChessBot/src/engine_core/board_state/pieces.h"

#define nsecs std::chrono::high_resolution_clock::now().time_since_epoch().count()

class LegalMoveGenTester
{
public:
    static void RunTests();

private:
    struct Test {
        std::string shortFen;
        uint8_t enPassant = Position::NONE;
        bool wlCastling = false;
        bool wsCastling = false;
        bool blCastling = false;
        bool bsCastling = false;
        Side side = Side::White;
        std::array<uint64_t, 6> nodes{};
    };

    static void RunTest(const Test& test);
    static uint64_t Perft(Position& position, Side side, uint32_t depth);
};
