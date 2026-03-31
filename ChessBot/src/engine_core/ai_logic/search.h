/************
* Search — iterative deepening with alpha-beta, principal variation, transposition table and quiescence search
* It uses move ordering (tt, promotions, captures, cutoff moves, history), late move reductions,
* Basic futility/razoring and mate-score normalization; terminology: cutoff moves, simple moves, halfmove
************/
#pragma once

#include <cstdint>
#include <chrono>

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "transposition_table.h"

struct SearchLimits {
    int max_depth = 64;
    int max_time_ms = 0;
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
    // Core search routines
    int AlphaBeta(Position& pos, int depth_left, int alpha, int beta, int ply, PvLine& pv);
    int Quiescence(Position& pos, int alpha, int beta, int ply, PvLine& pv);

    // Time / stop helpers
    bool IsTimeUp() const noexcept;
    bool CheckStopCondition() noexcept;

    // Mate-score normalization for transposition table
    static bool IsMateScore(int score) noexcept;
    static int ScoreToTT(int score, int ply) noexcept;
    static int ScoreFromTT(int score, int ply) noexcept;

    // Helper predicates for move classification
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

    // Simple move = not a capture and not a promotion
    inline static bool IsSimpleMove(const Move& m) {
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

    // Compact 16-bit key: from | (to << 8)
    inline static uint16_t FromToKey(const Move& m) {
        return static_cast<uint16_t>(m.GetFrom() | (m.GetTo() << 8));
    }

    inline bool IncreaseNodeCounter() noexcept {
        if (search_aborted_) {
            return false;
        }
        // Pre-check node limit to avoid crossing it
        if (limits_.nodes_limit > 0 && nodes_ >= limits_.nodes_limit) {
            search_aborted_ = true;
            return false;
        }
        ++nodes_;

        if ((nodes_ & kStopCheckMask) == 0) {
            if (CheckStopCondition()) {
                return false;
            }
        }

        return true;
    }

private:
    using SearchClock = std::chrono::steady_clock;

    TranspositionTable& tt_;
    bool (*is_stopped_)() = nullptr;

    static constexpr int64_t kStopCheckMask = 1023; // The number of nodes after which CheckStopCondition is executed

    int64_t nodes_ = 0;
    uint16_t cutoff_keys_[256][2]{}; // Two cutoff moves per ply (0 = empty)
    int history_[2][64][64]{};       // Simple move history (side, from, to)

    int lmr_base_index_ = 4;         // Start LMR from the 4th simple move

    SearchLimits limits_;
    bool search_aborted_ = false;
    bool has_time_limit_ = false;
    SearchClock::time_point deadline_;


    friend class SearchEngineTest;
};
