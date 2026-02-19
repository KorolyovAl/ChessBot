/************
* GameControllerQt — Qt adapter wrapping the GameController class.
* It converts controller callbacks into Qt signals consumable by MainWindow.
* The adapter does not contain game logic and keeps UI logic separated.
* Use this class to bridge engine events to Qt widgets and back.
************/
#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class GameController;

class GameControllerQt : public QObject {
    Q_OBJECT

public:
    explicit GameControllerQt(GameController& controller, QObject* parent = nullptr);
    explicit GameControllerQt(QObject* parent = nullptr);

    // UI commands (called from UI thread)
    Q_INVOKABLE void NewGame(bool white_engine, bool black_engine);
    Q_INVOKABLE void LoadFEN(const QString& fen, bool white_engine, bool black_engine);
    Q_INVOKABLE bool MakeUserMove(int from, int to, int promo_piece_type = 0);
    Q_INVOKABLE void RequestLegalMask(int square);
    Q_INVOKABLE void SetEngineDepthLimit(int max_depth);

signals:
    // UI updates (emitted by adapter on controller events)
    void BoardSnapshot(const QByteArray& pieces, bool white_to_move,
                       int last_from, int last_to, quint64 legal_mask);
    void PositionUpdated(const QString& fen);
    void MoveMade(int from, int to, int eval_centipawn);
    void SearchInfo(int depth, int eval_centipawn, const QString& principal_variation);
    void BestMove(int from, int to, const QString& principal_variation);
    void GameOver(int result, const QString& reason);
    void LegalMask(int square, quint64 mask);

private:
    void EmitSnapshotFromPosition();
    QByteArray BuildPiecesArrayFromEngine() const; // uses board_state tools

private:
    GameController* controller_ = nullptr;
};
