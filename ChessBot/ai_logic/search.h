/************
* Search — iterative deepening + alpha-beta with PV, TT, qsearch,
* move ordering (TT/Promos/Captures/CutoffMoves/History), LMR,
* basic futility/razoring, mate-score normalization. Terminology:
* CutoffMoves, SimpleMoves, halfmove.
************/
#pragma once

#include <cstdint>

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "transposition_table.h"

struct SearchLimits {
    int max_depth = 64;
    int64_t nodes_limit = 0; // 0 = unlimited
};

struct PvLine {
    Move moves[128];
    int length = 0;
};

struct SearchResult {
    Move best_move{};
    int score_cp = 0;
    int depth = 0;
    int64_t nodes = 0;
    PvLine pv;
};

class SearchEngine {
public:
    explicit SearchEngine(TranspositionTable& tt);

    void SetStopCallback(bool (*is_stopped)()) noexcept;

    SearchResult Search(Position& root, const SearchLimits& limits);

private:
    // Core searches
    int AlphaBeta(Position& pos, int depth, int alpha, int beta, int halfmove, PvLine& pv);
    int Quiescence(Position& pos, int alpha, int beta, int halfmove, PvLine& pv);

    // Service
    bool IsTimeUp() const noexcept;

    // Mate-score normalization for TT
    static bool IsMateScore(int score) noexcept;
    static int ScoreToTT(int score, int halfmove) noexcept;
    static int ScoreFromTT(int score, int halfmove) noexcept;

    // Helpers tied
    inline static bool IsPromotionFlag(Move::Flag f) {
        // Promotion flags are a closed set
        switch (f) {
        case Move::Flag::PromoteToKnight: {
            return true;
        }
        case Move::Flag::PromoteToBishop: {
            return true;
        }
        case Move::Flag::PromoteToRook: {
            return true;
        }
        case Move::Flag::PromoteToQueen: {
            return true;
        }
        default: {
            return false;
        }
        }
    }

    inline static bool IsSimpleMove(const Move& m) {
        // SimpleMove = not a capture and not a promotion
        const auto f = m.GetFlag();
        if (f == Move::Flag::Capture) {
            return false;
        }
        if (f == Move::Flag::EnPassantCapture) {
            return false;
        }
        if (IsPromotionFlag(f)) {
            return false;
        }
        return true;
    }

    void ResetCutoffKeys() noexcept;

    inline static uint16_t FromToKey(const Move& m) {
        // Compact 16-bit key: from | (to << 8)
        return static_cast<uint16_t>(m.GetFrom() | (m.GetTo() << 8));
    }

private:
    TranspositionTable& tt_;
    bool (*is_stopped_)() = nullptr;

    int64_t nodes_ = 0;
    uint16_t cutoff_keys_[256][2]{}; // two CutoffMoves per halfmove (0 = empty)
    int history_[2][64][64]{};       // SimpleMoves history (side, from, to)

    int lmr_base_index_ = 4;         // start LMR from the 4th SimpleMove
};
