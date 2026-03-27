/************
* MainWindow hosts BoardWidget and wires it to GameControllerQt. The controller
* provides a board snapshot for rendering; FEN may still be used for logging.
************/

#pragma once

#include <QMainWindow>
#include <QPointer>

namespace engine_runtime {
class EngineRuntime;
}

class BoardWidget;
class GameControllerQt;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(engine_runtime::EngineRuntime& runtime, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void OnBoardSnapshot(const QByteArray& pieces, bool white_to_move,
                         int last_from, int last_to, quint64 legal_mask);
    void OnMoveMade(int from_sq, int to_sq, int eval_centipawn);
    void OnBestMove(int from_sq, int to_sq, const QString& principal_variation);
    void OnGameOver(int result, const QString& reason);

private:
    void BuildUi();
    void WireSignals();

private:
    BoardWidget* board_widget_ = nullptr;
    QPointer<GameControllerQt> controller_qt_;
};
