/************
* BoardWidget renders a chessboard and handles user input. It receives a flat
* board snapshot prepared by the controller, plus auxiliary flags for highlights.
* The widget emits requests for legal masks and finalized user moves.
************/

#pragma once

#include <QWidget>
#include <QByteArray>
#include <QPixmap>
#include <array>
#include <optional>

class QMouseEvent;
class QPaintEvent;

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(QWidget* parent = nullptr);

    // Snapshot-based update API (no engine logic inside the view)
    void SetSnapshot(const QByteArray& pieces, bool white_to_move,
                     int last_from, int last_to, quint64 legal_mask);

    void SetOrientationWhiteBottom(bool white_bottom);
    void ClearSelection();
    void SetShowCoords(bool show);
    void SetInCheckSquare(int sq);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void RequestLegalMask(int sq);
    void MoveChosen(int from_sq, int to_sq, int promo_ui_code);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void RecomputeGeometry();
    int CellSize() const;

    int PixelToSquare(const QPoint& p) const;
    QRect SquareRect(int sq) const;
    void DrawBoard(QPainter& p) const;
    void DrawHighlights(QPainter& p) const;
    void DrawPieces(QPainter& p) const;
    void DrawCoords(QPainter& p) const;
    void EnsurePiecePixmapsLoaded() const;
    int AskPromotionPiece(bool white_to_move) const;

private:
    QRect window_;
    QRect board_;
    QRect x_coords_;
    QRect y_coords_;
    //QRect timer_;
    //QRect evaluate_bar_;

    QByteArray pieces_; // 64 bytes: 0=None, 1..12 per-piece code (controller-defined)
    bool white_to_move_ {true};
    int last_from_ = -1;
    int last_to_ = -1;
    quint64 legal_mask_ = 0;

    std::optional<int> selected_sq_;
    int in_check_sq_ = -1;

    bool white_bottom_ {true};
    bool show_coords_ {true};

    mutable std::array<QPixmap, 12> piece_pix_; // WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK
};
