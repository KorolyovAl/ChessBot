/************
* GameControllerQt — Qt adapter wrapping the GameController class.
* It converts controller callbacks into Qt signals consumable by MainWindow.
* The adapter does not contain game logic and keeps UI logic separated.
* Use this class to bridge engine events to Qt widgets and back.
************/
#pragma once


#include <QObject>

class GameController;

class GameControllerQt : public QObject {
    Q_OBJECT

public:
    explicit GameControllerQt(GameController& controller, QObject* parent = nullptr);

    Q_INVOKABLE void NewGame();
    Q_INVOKABLE void LoadFEN(const QString& fen);
    Q_INVOKABLE bool MakeUserMove(int from, int to, int promoPieceType = 0);

signals:
    void PositionUpdated(const QString& fen);
    void MoveMade(int from, int to, int eval_centipawn);
    void SearchInfo(int depth, int eval_centipawn, const QString& principal_variation);
    void BestMove(int from, int to, const QString& principal_variation);
    void GameOver(int result, const QString& reason);
    void LegalMask(int square, quint64 mask);
private:
    GameController* controller_ = nullptr;
};
