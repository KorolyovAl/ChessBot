#include "engine_runtime.h"

#include <utility>

namespace engine_runtime {

EngineRuntime::EngineRuntime(QObject& ui_anchor, size_t tt_size_hash)
    : table_(tt_size_hash)
    , controller_(table_)
    , dispatcher_(ui_anchor)
{
    //controller_.SetEngineLimits();
}

EngineRuntime::~EngineRuntime() {
    Stop();
}

void EngineRuntime::Start() {
    bool expected = false;
    if (!is_started_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_ = std::jthread([this]{
        WorkerLoop();
    });
}

void EngineRuntime::Stop() {
    bool expected = true;
    if (!is_started_.compare_exchange_strong(expected, false)) {
        return;
    }

    queue_.Close();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void EngineRuntime::NewGame(const Players& players, const TimeControl& tc) {
    PostTask([this, players, tc]() {
        last_from_ = -1;
        last_to_ = -1;
        legal_mask_ = 0ULL;

        controller_.NewGame(players, tc);
        DispatchSnapshot();
        RunPendingEngineTurn();
    });
}

void EngineRuntime::LoadFEN(std::string short_fen, const Players& players, const TimeControl& tc) {
    PostTask([this, short_fen = std::move(short_fen), players, tc]() mutable {
        last_from_ = -1;
        last_to_ = -1;
        legal_mask_ = 0ULL;

        controller_.LoadFEN(short_fen, players, tc);
        DispatchSnapshot();
        RunPendingEngineTurn();
    });
}

void EngineRuntime::MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type) {
    PostTask([this, from, to, promo_piece_type]() {
        const MoveOutcome outcome = controller_.MakeUserMove(from, to, promo_piece_type);
        if (!outcome.is_valid) {
            return;
        }

        last_from_ = static_cast<int>(outcome.move.GetFrom());
        last_to_ = static_cast<int>(outcome.move.GetTo());
        legal_mask_ = 0ULL;

        DispatchMove(last_from_, last_to_, outcome.eval_cp);
        DispatchSnapshot();

        if (outcome.result != GameResult::Ongoing) {
            DispatchGameOver(outcome.result, controller_.GetResultReason());
            return;
        }

        RunPendingEngineTurn();
    });
}

void EngineRuntime::RequestLegalMask(uint8_t square) {
    PostTask([this, square]() {
        legal_mask_ = controller_.GetLegalMask(square);
        DispatchLegalMask(square, legal_mask_);
        DispatchSnapshot();
    });
}

void EngineRuntime::SetEngineLimits(const EngineLimits& limits) {
    PostTask([this, limits]() {
        controller_.SetEngineLimits(limits);
    });
}

void EngineRuntime::SetOnSnapshot(OnSnapshot callback) {
    on_snapshot_ = std::move(callback);
}

void EngineRuntime::SetOnMove(OnMove callback) {
    on_move_ = std::move(callback);
}

void EngineRuntime::SetOnSearchInfo(OnSearchInfo callback) {
    on_search_info_ = std::move(callback);
}

void EngineRuntime::SetOnBestMove(OnBestMove callback) {
    on_best_move_ = std::move(callback);
}

void EngineRuntime::SetOnGameOver(OnGameOver callback) {
    on_game_over_ = std::move(callback);
}

void EngineRuntime::SetOnLegalMask(OnLegalMask callback) {
    on_legal_mask_ = std::move(callback);
}

bool EngineRuntime::PostTask(Task task) {
    if (!is_started_.load()) {
        return false;
    }

    return queue_.Push(std::move(task));
}

void EngineRuntime::WorkerLoop() {
    Task task;
    while (queue_.Pop(task)) {
        if (task) {
            task();
        }
    }
}

void EngineRuntime::RunPendingEngineTurn() {
    if (!controller_.HasPosition()) {
        return;
    }

    if (!controller_.IsEngineToMove()) {
        return;
    }

    const EngineTurnOutcome outcome = controller_.RunEngineTurn();
    if (!outcome.best_move_found) {
        if (outcome.result != GameResult::Ongoing) {
            DispatchSnapshot();
            DispatchGameOver(outcome.result, controller_.GetResultReason());
        }
        return;
    }

    last_from_ = static_cast<int>(outcome.best_move.GetFrom());
    last_to_ = static_cast<int>(outcome.best_move.GetTo());
    legal_mask_ = 0ULL;

    DispatchBestMove(last_from_, last_to_, outcome.principal_variation);
    DispatchMove(last_from_, last_to_, outcome.eval_cp);
    DispatchSnapshot();

    if (outcome.result != GameResult::Ongoing) {
        DispatchGameOver(outcome.result, controller_.GetResultReason());
    }
}

void EngineRuntime::DispatchSnapshot() {
    auto callback = on_snapshot_;
    if (!callback) {
        return;
    }

    BoardSnapshot snapshot = BuildSnapshotFromController();
    dispatcher_.Post([callback = std::move(callback), snapshot = std::move(snapshot)]() mutable {
        callback(snapshot);
    });
}

void EngineRuntime::DispatchMove(int from, int to, int eval_centipawn) {
    auto callback = on_move_;
    if (!callback) {
        return;
    }

    dispatcher_.Post([callback = std::move(callback), from, to, eval_centipawn]() mutable {
        callback(from, to, eval_centipawn);
    });
}

void EngineRuntime::DispatchBestMove(int from, int to, const std::string& pv) {
    auto callback = on_best_move_;
    if (!callback) {
        return;
    }

    dispatcher_.Post([callback = std::move(callback), from, to, pv]() mutable {
        callback(from, to, pv);
    });
}

void EngineRuntime::DispatchGameOver(GameResult result, const std::string& reason) {
    auto callback = on_game_over_;
    if (!callback) {
        return;
    }

    dispatcher_.Post([callback = std::move(callback), result, reason]() mutable {
        callback(result, reason);
    });
}

void EngineRuntime::DispatchLegalMask(uint8_t square, uint64_t mask) {
    auto callback = on_legal_mask_;
    if (!callback) {
        return;
    }

    dispatcher_.Post([callback = std::move(callback), square, mask]() mutable {
        callback(square, mask);
    });
}

BoardSnapshot EngineRuntime::BuildSnapshotFromController() const {
    BoardSnapshot snapshot{};

    if (!controller_.HasPosition()) {
        return snapshot;
    }

    for (int sq = 0; sq < 64; ++sq) {
        snapshot.pieces[sq] = static_cast<uint8_t>(controller_.GetPiece(sq));
    }

    snapshot.white_to_move = controller_.IsWhiteToMove();
    snapshot.last_from = last_from_;
    snapshot.last_to = last_to_;
    snapshot.legal_mask = legal_mask_;
    snapshot.fen = controller_.GetFEN();

    return snapshot;
}

} // namespace engine_runtime

