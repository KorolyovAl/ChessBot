#include "game_controller_qt.h"
#include "../game_controller/game_controller.h"

#include <QString>

GameControllerQt::GameControllerQt(GameController& controller, QObject* parent)
    : QObject(parent)
    , controller_(&controller) {
}

void GameControllerQt::NewGame() {
}

void GameControllerQt::LoadFEN(const QString& fen) {
    (void)fen;
}

bool GameControllerQt::MakeUserMove(int from, int to, int promoPieceType) {
    (void)from;
    (void)to;
    (void)promoPieceType;
    return false;
}
