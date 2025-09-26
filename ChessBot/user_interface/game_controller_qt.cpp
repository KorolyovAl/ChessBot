#include "game_controller_qt.h"
#include "../game_controller/game_controller.h"
#include "../board_state/move.h"

#include <QString>

GameControllerQt::GameControllerQt(GameController& controller, QObject* parent)
    : QObject(parent)
    , controller_(&controller) {

    // Bridge pure-controller callbacks to Qt signals
    if (controller_ != nullptr) {
        // Emit current FEN so UI can redraw the board/state.
        controller_->SetOnPosition([this](const Position& /*position*/) {
            if (controller_ != nullptr) {
                const QString fen = QString::fromStdString(controller_->GetFEN());
                emit PositionUpdated(fen);
                EmitSnapshotFromPosition();
            }
        });

        // Notify UI that a move was applied (from/to squares and current eval).
        controller_->SetOnMove([this](const Move& move, int halfmove_index, int eval_centipawns) {
            (void)halfmove_index;
            const int from = static_cast<int>(move.GetFrom());
            const int to = static_cast<int>(move.GetTo());
            emit MoveMade(from, to, eval_centipawns);
        });

        // Periodic search info (depth, eval, PV line).
        controller_->SetOnSearchInfo([this](int depth_halfmoves, int eval_centipawns, const std::string& principal_variation) {
            const QString pv = QString::fromStdString(principal_variation);
            emit SearchInfo(depth_halfmoves, eval_centipawns, pv);
        });

        // Final best move of the search iteration (or full search).
        controller_->SetOnBestMove([this](const Move& best_move, const std::string& principal_variation) {
            const int from = static_cast<int>(best_move.GetFrom());
            const int to = static_cast<int>(best_move.GetTo());
            const QString pv = QString::fromStdString(principal_variation);
            emit BestMove(from, to, pv);
        });

        // Game finished: send code and short reason to UI.
        controller_->SetOnGameOver([this](GameResult result, const std::string& reason_text) {
            const int code = static_cast<int>(result);
            const QString reason = QString::fromStdString(reason_text);
            emit GameOver(code, reason);
        });

        // Bitmask of legal targets for a given origin square; used for board highlighting.
        controller_->SetOnLegalMask([this](uint8_t square, uint64_t legal_moves_mask) {
            emit LegalMask(static_cast<int>(square), static_cast<quint64>(legal_moves_mask));
        });
    }
}

GameControllerQt::GameControllerQt(QObject* parent)
    : QObject(parent) {
}

void GameControllerQt::NewGame(bool white_engine, bool black_engine) {
    if (controller_ == nullptr) {
        return;
    }

    Players players;
    players.white = white_engine ? PlayerType::Engine : PlayerType::Human;
    players.black = black_engine ? PlayerType::Engine : PlayerType::Human;

    TimeControl tc;
    tc.base_ms = 300'000; // 5 minutes
    tc.increment_ms = 300; // +3 seconds
    tc.use_increment = true;

    controller_->NewGame(players, tc);

    EmitSnapshotFromPosition();
}

void GameControllerQt::LoadFEN(const QString& fen, bool white_engine, bool black_engine) {
    if (controller_ == nullptr) {
        return;
    }

    Players players;
    players.white = white_engine ? PlayerType::Engine : PlayerType::Human;
    players.black = black_engine ? PlayerType::Engine : PlayerType::Human;

    TimeControl tc;
    tc.base_ms = 300'000;
    tc.increment_ms = 300;
    tc.use_increment = true;

    controller_->LoadFEN(fen.toStdString(), players, tc);

    EmitSnapshotFromPosition();
}

bool GameControllerQt::MakeUserMove(int from, int to, int promo_piece_type) {
    if (controller_ == nullptr) {
        return false;
    }

    const uint8_t ufrom = static_cast<uint8_t>(from);
    const uint8_t uto = static_cast<uint8_t>(to);
    const uint8_t upromo = static_cast<uint8_t>(promo_piece_type);

    const bool ok = controller_->MakeUserMove(ufrom, uto, upromo);
    if (ok) {
        EmitSnapshotFromPosition();
    }
    return ok;
}

void GameControllerQt::RequestLegalMask(int square) {
    if (controller_ == nullptr) {
        return;
    }

    const uint8_t sq = static_cast<uint8_t>(square);
    controller_->RequestLegalMask(sq);
}

void GameControllerQt::SetEngineDepthLimit(int max_depth) {
    if (controller_ == nullptr) {
        return;
    }

    EngineLimits lim = {};
    if (max_depth > 0) {
        lim.max_depth = max_depth;
    }
    controller_->SetEngineLimits(lim);
}

void GameControllerQt::EmitSnapshotFromPosition() {
    const QByteArray pieces = BuildPiecesArrayFromEngine();
    const bool white_to_move = true; // Obtain from Position (move_counter_ rule)
    const int last_from = -1; // Fill from controller's last move
    const int last_to = -1;
    const quint64 legal_mask = 0; // Optionally pass last requested mask

    emit BoardSnapshot(pieces, white_to_move, last_from, last_to, legal_mask);
}

QByteArray GameControllerQt::BuildPiecesArrayFromEngine() const {
    QByteArray arr(64, '\0');

    for (int sq = 0; sq < 64; ++sq) {
        arr[sq] = controller_->GetPiece(sq);
    }
    return arr;
}
