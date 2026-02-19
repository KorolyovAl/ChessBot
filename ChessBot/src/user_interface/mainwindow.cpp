#include "mainwindow.h"
#include "board_widget.h"
#include "game_controller_qt.h"

#include <QMenuBar>
#include <QAction>
#include <QStatusBar>

MainWindow::MainWindow(GameController* controller, QWidget* parent)
    : QMainWindow(parent)
{
    controller_qt_ = new GameControllerQt(*controller);

    BuildUi();
    WireSignals();
    if (controller_qt_ != nullptr) {
        controller_qt_->NewGame(false, true);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::BuildUi() {
    board_widget_ = new BoardWidget(this);
    setCentralWidget(board_widget_);

    QMenu* game_menu = menuBar()->addMenu("Game");
    QAction* new_game_white = game_menu->addAction("New Game: You (White)");
    QAction* new_game_black = game_menu->addAction("New Game: You (Black)");

    connect(new_game_white, &QAction::triggered, this, [this]() {
        if (controller_qt_) {
            controller_qt_->NewGame(false, true);
        }

        if (board_widget_ != nullptr) {
            board_widget_->SetOrientationWhiteBottom(true);
            board_widget_->ClearSelection();
        }
    });

    connect(new_game_black, &QAction::triggered, this, [this]() {
        if (controller_qt_) {
            controller_qt_->NewGame(true, false);
        }
        if (board_widget_ != nullptr) {
            board_widget_->SetOrientationWhiteBottom(false);
            board_widget_->ClearSelection();
        }
    });

    statusBar()->showMessage("Ready");
}

void MainWindow::WireSignals() {
    if (controller_qt_ == nullptr) {
        return;
    }

    connect(controller_qt_, &GameControllerQt::BoardSnapshot,
            this, &MainWindow::OnBoardSnapshot);
    connect(controller_qt_, &GameControllerQt::MoveMade,
            this, &MainWindow::OnMoveMade);
    connect(controller_qt_, &GameControllerQt::BestMove,
            this, &MainWindow::OnBestMove);

    if (board_widget_ != nullptr) {
        connect(board_widget_, &BoardWidget::RequestLegalMask,
                controller_qt_, &GameControllerQt::RequestLegalMask);
        connect(board_widget_, &BoardWidget::MoveChosen, controller_qt_,
                [this](int from_sq, int to_sq, int promo_ui) {
                    if (controller_qt_) {
                        controller_qt_->MakeUserMove(from_sq, to_sq, promo_ui);
                    }
                });
    }
}

void MainWindow::OnBoardSnapshot(const QByteArray& pieces, bool white_to_move,
                                 int last_from, int last_to, quint64 legal_mask) {
    if (board_widget_ != nullptr) {
        board_widget_->SetSnapshot(pieces, white_to_move, last_from, last_to, legal_mask);
    }
}

void MainWindow::OnMoveMade(int from_sq, int to_sq) {
    if (board_widget_ != nullptr) {
        board_widget_->ClearSelection();
    }
}

void MainWindow::OnBestMove(int from_sq, int to_sq) {
    if (board_widget_ != nullptr) {
        // Function keeps last move highlight in sync; actual board comes via snapshot signal.
        board_widget_->SetInCheckSquare(-1);
    }
}
