#include <QApplication>

#include "user_interface/mainwindow.h"
#include "ai_logic/transposition_table.h"
#include "game_controller/game_controller.h"
#include "user_interface/game_controller_qt.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    TranspositionTable tt(128);             // 128 MB cash
    GameController controller(tt);

    MainWindow window(&controller);
    window.show();

    return app.exec();
}
