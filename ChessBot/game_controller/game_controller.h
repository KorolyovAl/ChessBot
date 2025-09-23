/************
* GameController — C++ orchestrator between UI and the chess engine core.
* It owns game lifecycle, position state, clocks, and exposes callback hooks.
* The class is UI-agnostic and does not depend on Qt; a thin Qt adapter can wrap it.
* This header defines the public API and lightweight data structures for control.
************/
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <memory>

class Position;
class Move;
class TranspositionTable;
class SearchEngine;
class RepetitionHistory;

enum class Side;

struct TimeControl {
    int base_ms = 300'000;
    int increment_ms = 300;
    bool use_increment = true;
};

struct EngineLimits {
    int max_depth = 0;
    int max_time_ms = 0;
    int max_nodes = 0;
};

enum class PlayerType {
    Human,
    Engine
};

struct Players {
    PlayerType white = PlayerType::Human;
    PlayerType black = PlayerType::Engine;
};

enum class GameResult : uint8_t {
    Ongoing,
    WhiteWon,
    BlackWon,
    DrawStalemate,
    DrawFiftyMove,
    DrawRepetition,
    DrawMaterial
};

enum class ControllerState : uint8_t {
    Null,
    Setup,
    PlayerTurn,
    EngineThinking,
    Paused,
    GameOver
};

class GameController {
public:
    explicit GameController(TranspositionTable& tt);

    void NewGame(const Players& players, const TimeControl& tc);
    void LoadFEN(const std::string& short_fen, const Players& players, const TimeControl& tc);

    bool MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type = 0);
    void RequestLegalMask(uint8_t square);

    void SetEngineLimits(const EngineLimits& lim);
    void SetEngineSide(Side side, bool enabled);
    void StopSearch();

    std::string GetFEN() const;
    GameResult GetResult() const;

    using OnPosition = std::function<void(const Position&)>;
    using OnMove = std::function<void(const Move&, int /*halfmove_index*/, int /*eval_centipawns*/)>;
    using OnSearchInfo = std::function<void(int /*depth*/, int /*eval_centipawns*/, const std::string& /*principal_variation*/)>;
    using OnBestMove = std::function<void(const Move&, const std::string& /*principal_variation*/)>;
    using OnGameOver = std::function<void(GameResult, const std::string& /*reason*/)>;
    using OnLegalMask = std::function<void(uint8_t /*square*/, uint64_t /*mask*/)>;

    void SetOnPosition(OnPosition callback);
    void SetOnMove(OnMove callback);
    void SetOnSearchInfo(OnSearchInfo callback);
    void SetOnBestMove(OnBestMove callback);
    void SetOnGameOver(OnGameOver callback);
    void SetOnLegalMask(OnLegalMask callback);

private:
    void EnterPlayerTurn_();
    void EnterEngineThinking_();
    void ApplyMoveAndNotify_(const Move& m, int eval_cp);
    void EmitPosition_() const;

private:
    TranspositionTable& table_;

    std::unique_ptr<Position> position_;
    std::unique_ptr<SearchEngine> engine_;
    std::unique_ptr<RepetitionHistory> repetition_;

    Players players_{};
    TimeControl time_control_{};
    EngineLimits engine_limits_{};

    ControllerState state_ = ControllerState::Null;
    GameResult result_ = GameResult::Ongoing;

    OnPosition on_position_{};
    OnMove on_move_{};
    OnSearchInfo on_search_info_{};
    OnBestMove on_best_move_{};
    OnGameOver on_game_over_{};
    OnLegalMask on_legal_mask_{};
};
