/************
* GameController - C++ orchestrator between UI and the chess engine core.
* It owns game lifecycle, position state, and exposes synchronous command methods.
* The class is UI-agnostic and does not depend on Qt or runtime dispatch helpers.
* This header defines the public API and lightweight data structures for control.
************/
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "../engine_core/board_state/move.h"
#include "../engine_core/board_state/pieces.h"
#include "../engine_core/ai_logic/search.h"

class Position;
class TranspositionTable;
class SearchEngine;

struct TimeControl {
    int base_ms = 300'00;
    int increment_ms = 300;
    bool use_increment = true;
};

struct EngineLimits {
    int max_depth = 10;
    int max_time_ms = 1'000;
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

struct MoveOutcome {
    bool is_valid = false;
    Move move{};
    int eval_cp = 0;
    GameResult result = GameResult::Ongoing;
};

struct EngineTurnOutcome {
    bool best_move_found = false;
    Move best_move{};
    int eval_cp = 0;
    std::string principal_variation;
    GameResult result = GameResult::Ongoing;
};

class GameController {
public:
    explicit GameController(TranspositionTable& tt);

    void NewGame(const Players& players, const TimeControl& tc);
    void LoadFEN(const std::string& short_fen, const Players& players, const TimeControl& tc);
    MoveOutcome MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type = 0);
    EngineTurnOutcome RunEngineTurn();
    uint64_t GetLegalMask(uint8_t square) const;

    void SetEngineLimits(const EngineLimits& lim);
    void SetEngineSide(Side side, bool enabled);

    bool HasPosition() const;
    bool IsWhiteToMove() const;
    bool IsEngineToMove() const;
    ControllerState GetState() const;

    std::string GetFEN() const;
    std::string GetResultReason() const;
    GameResult GetResult() const;
    int GetPiece(int square) const;

private:
    void UpdateStateAfterTurn();

private:
    TranspositionTable& table_;

    std::unique_ptr<Position> position_;
    std::unique_ptr<SearchEngine> engine_;

    Players players_{};
    TimeControl time_control_{};
    EngineLimits engine_limits_{};

    ControllerState state_ = ControllerState::Null;
    GameResult result_ = GameResult::Ongoing;
};
