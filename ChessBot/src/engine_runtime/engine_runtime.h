#pragma once

#include <QObject>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "task_queue.h"
#include "dispatcher_ui.h"
#include "../engine_core/ai_logic/transposition_table.h"
#include "../game_controller/game_controller.h"

namespace engine_runtime {

struct BoardSnapshot {
    std::array<uint8_t, 64> pieces{};
    bool white_to_move = true;
    int last_from = -1;
    int last_to = -1;
    uint64_t legal_mask = 0ULL;
    std::string fen;
};

class EngineRuntime {
public:
    using Task = std::function<void()>;

    using OnSnapshot = std::function<void(const BoardSnapshot)>;
    using OnMove = std::function<void(int from, int to, int eval_centipawn)>;
    using OnSearchInfo = std::function<void(int depth, int eval_centipawn, const std::string&)>;
    using OnBestMove = std::function<void(int from, int to, const std::string&)>;
    using OnGameOver = std::function<void(GameResult result, const std::string&)>;
    using OnLegalMask = std::function<void(uint8_t square, uint64_t mask)>;

public:
    explicit EngineRuntime(QObject& ui_anchor, size_t tt_size_hash = 64 /*MB*/);
    ~EngineRuntime();

    EngineRuntime(const EngineRuntime&) = delete;
    EngineRuntime& operator=(const EngineRuntime&) = delete;

    void Start();
    void Stop();

    void NewGame(const Players& players, const TimeControl& tc);
    void LoadFEN(std::string short_fen, const Players& players, const TimeControl& tc);
    void MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type);
    void RequestLegalMask(uint8_t square);
    void SetEngineLimits(const EngineLimits& limits);

    void SetOnSnapshot(OnSnapshot callback);
    void SetOnMove(OnMove callback);
    void SetOnSearchInfo(OnSearchInfo callback);
    void SetOnBestMove(OnBestMove callback);
    void SetOnGameOver(OnGameOver callback);
    void SetOnLegalMask(OnLegalMask callback);

private:
    bool PostTask(Task task);
    void WorkerLoop();

    void RunPendingEngineTurn();
    void DispatchSnapshot();
    void DispatchMove(int from, int to, int eval_centipawn);
    void DispatchBestMove(int from, int to, const std::string& pv);
    void DispatchGameOver(GameResult result, const std::string& reason);
    void DispatchLegalMask(uint8_t square, uint64_t mask);

    BoardSnapshot BuildSnapshotFromController() const;

private:
    TranspositionTable table_;
    GameController controller_;

    TaskQueue<Task> queue_;
    UiDispatcher dispatcher_;
    std::jthread worker_;

    std::atomic<bool> is_started_ = false;

    int last_from_ = -1;
    int last_to_ = -1;
    uint64_t legal_mask_ = 0ULL;

    // callbacks
    OnSnapshot on_snapshot_;
    OnMove on_move_;
    OnSearchInfo on_search_info_;
    OnBestMove on_best_move_;
    OnGameOver on_game_over_;
    OnLegalMask on_legal_mask_;
};

} // namespace engine_runtime
