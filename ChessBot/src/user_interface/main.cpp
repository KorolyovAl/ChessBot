#include <QApplication>

#include "mainwindow.h"
#include "../engine_runtime/engine_runtime.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    engine_runtime::EngineRuntime runtime(app, 128);
    runtime.Start();

    MainWindow window(runtime);
    window.show();

    const int exit_code = app.exec();
    runtime.Stop();
    return exit_code;
}
