#include "game_controller_qt.h"

#include <QString>
#include <QPointer>

namespace {

QByteArray ToQByteArray(const engine_runtime::BoardSnapshot& snapshot) {
    QByteArray pieces(64, '\0');
    for (int sq = 0; sq < 64; ++sq) {
        pieces[sq] = static_cast<char>(snapshot.pieces[sq]);
    }
    return pieces;
}

} // namespace

GameControllerQt::GameControllerQt(engine_runtime::EngineRuntime& runtime, QObject* parent)
    : QObject(parent)
    , runtime_(runtime) {
    QPointer<GameControllerQt> self(this);

    runtime_.SetOnSnapshot([self](const engine_runtime::BoardSnapshot& snapshot) {
        if (!self) {
            return;
        }

        emit self->BoardSnapshot(ToQByteArray(snapshot),
                                 snapshot.white_to_move,
                                 snapshot.last_from,
                                 snapshot.last_to,
                                 static_cast<quint64>(snapshot.legal_mask));
        emit self->PositionUpdated(QString::fromStdString(snapshot.fen));
    });

    runtime_.SetOnMove([self](int from, int to, int eval_centipawn) {
        if (!self) {
            return;
        }

        emit self->MoveMade(from, to, eval_centipawn);
    });

    runtime_.SetOnBestMove([self](int from, int to, const std::string& principal_variation) {
        if (!self) {
            return;
        }

        emit self->BestMove(from, to, QString::fromStdString(principal_variation));
    });

    runtime_.SetOnGameOver([self](GameResult result, const std::string& reason) {
        if (!self) {
            return;
        }

        emit self->GameOver(static_cast<int>(result), QString::fromStdString(reason));
    });

    runtime_.SetOnLegalMask([self](uint8_t square, uint64_t mask) {
        if (!self) {
            return;
        }

        emit self->LegalMask(static_cast<int>(square), static_cast<quint64>(mask));
    });
}

GameControllerQt::~GameControllerQt() {
    runtime_.SetOnSnapshot({});
    runtime_.SetOnMove({});
    runtime_.SetOnSearchInfo({});
    runtime_.SetOnBestMove({});
    runtime_.SetOnGameOver({});
    runtime_.SetOnLegalMask({});
}

void GameControllerQt::NewGame(bool white_engine, bool black_engine) {
    Players players;
    players.white = white_engine ? PlayerType::Engine : PlayerType::Human;
    players.black = black_engine ? PlayerType::Engine : PlayerType::Human;

    TimeControl time_control;
    time_control.base_ms = 30'000;
    time_control.increment_ms = 300;
    time_control.use_increment = true;

    runtime_.NewGame(players, time_control);
}

void GameControllerQt::LoadFEN(const QString& fen, bool white_engine, bool black_engine) {
    Players players;
    players.white = white_engine ? PlayerType::Engine : PlayerType::Human;
    players.black = black_engine ? PlayerType::Engine : PlayerType::Human;

    TimeControl time_control;
    time_control.base_ms = 30'000;
    time_control.increment_ms = 300;
    time_control.use_increment = true;

    runtime_.LoadFEN(fen.toStdString(), players, time_control);
}

void GameControllerQt::MakeUserMove(int from, int to, int promo_piece_type) {
    runtime_.MakeUserMove(static_cast<uint8_t>(from),
                          static_cast<uint8_t>(to),
                          static_cast<uint8_t>(promo_piece_type));
}

void GameControllerQt::RequestLegalMask(int square) {
    runtime_.RequestLegalMask(static_cast<uint8_t>(square));
}

void GameControllerQt::SetEngineDepthLimit(int max_depth) {
    EngineLimits limits{};
    if (max_depth > 0) {
        limits.max_depth = max_depth;
    }
    runtime_.SetEngineLimits(limits);
}
