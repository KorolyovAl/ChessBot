#include "board_widget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QStyleOption>
#include <QInputDialog>
#include <QFont>

BoardWidget::BoardWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void BoardWidget::SetSnapshot(const QByteArray& pieces, bool white_to_move,
                              int last_from, int last_to, quint64 legal_mask) {
    if (pieces.size() != 64) {
        return;
    }
    pieces_ = pieces;
    white_to_move_ = white_to_move;
    last_from_ = last_from;
    last_to_ = last_to;
    legal_mask_ = legal_mask;
    update();
}

void BoardWidget::SetOrientationWhiteBottom(bool white_bottom) {
    white_bottom_ = white_bottom;
    update();
}

void BoardWidget::ClearSelection() {
    selected_sq_.reset();
    legal_mask_ = 0;
    update();
}

void BoardWidget::SetShowCoords(bool show) {
    show_coords_ = show;
    update();
}

void BoardWidget::SetInCheckSquare(int sq) {
    in_check_sq_ = sq;
    update();
}

QSize BoardWidget::sizeHint() const {
    return {640, 640};
}

QSize BoardWidget::minimumSizeHint() const {
    return {320, 320};
}

void BoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    DrawBoard(p);
    DrawHighlights(p);
    DrawPieces(p);
    if (show_coords_) {
        DrawCoords(p);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const int sq = PixelToSquare(event->pos());
    if (sq < 0 || sq >= 64) {
        return;
    }

    if (!selected_sq_.has_value()) {
        selected_sq_ = sq;
        emit RequestLegalMask(sq);
        update();
        return;
    }
    else {
        const int from_sq = *selected_sq_;
        const int to_sq = sq;
        int promo = 0;

        if (pieces_.size() == 64) {
            const unsigned char moving = static_cast<unsigned char>(pieces_.at(from_sq));
            const int rank_to = to_sq / 8;
            const bool is_white_pawn = (moving == 1);
            const bool is_black_pawn = (moving == 7);
            if ((is_white_pawn && rank_to == 7) || (is_black_pawn && rank_to == 0)) {
                promo = AskPromotionPiece(white_to_move_);
            }
        }

        emit MoveChosen(from_sq, to_sq, promo);
        selected_sq_.reset();
        legal_mask_ = 0;
        update();
        return;
    }
}

int BoardWidget::PixelToSquare(const QPoint& p) const {
    const int w = width();
    const int h = height();
    const int size = qMin(w, h);
    const int off_x = (w - size) / 2;
    const int off_y = (h - size) / 2;

    if (p.x() < off_x || p.y() < off_y || p.x() >= off_x + size || p.y() >= off_y + size) {
        return -1;
    }

    const int cell = size / 8;
    const int file = (p.x() - off_x) / cell;
    const int rank = (p.y() - off_y) / cell;

    int draw_file = file;
    int draw_rank = rank;
    if (!white_bottom_) {
        draw_file = 7 - draw_file;
        draw_rank = 7 - draw_rank;
    }

    const int sq_top_left = draw_rank * 8 + draw_file;
    const int sq = (7 - (sq_top_left / 8)) * 8 + (sq_top_left % 8);
    return sq;
}

QRect BoardWidget::SquareRect(int sq) const {
    const int w = width();
    const int h = height();
    const int size = qMin(w, h);
    const int off_x = (w - size) / 2;
    const int off_y = (h - size) / 2;
    const int cell = size / 8;

    int engine_rank = sq / 8;
    int engine_file = sq % 8;

    int draw_rank = 7 - engine_rank;
    int draw_file = engine_file;
    if (!white_bottom_) {
        draw_rank = 7 - draw_rank;
        draw_file = 7 - draw_file;
    }

    const int x = off_x + draw_file * cell;
    const int y = off_y + draw_rank * cell;
    return QRect(x, y, cell, cell);
}

void BoardWidget::DrawBoard(QPainter& p) const {
    const int w = width();
    const int h = height();
    const int size = qMin(w, h);
    const int off_x = (w - size) / 2;
    const int off_y = (h - size) / 2;
    const int cell = size / 8;

    for (int r = 0; r < 8; ++r) {
        for (int f = 0; f < 8; ++f) {
            const bool light = ((r + f) % 2 == 0);
            QColor col = light ? QColor(240, 217, 181) : QColor(181, 136, 99);
            p.fillRect(off_x + f * cell, off_y + r * cell, cell, cell, col);
        }
    }

    p.setPen(QPen(QColor(60, 60, 60)));
    for (int i = 0; i <= 8; ++i) {
        p.drawLine(off_x, off_y + i * cell, off_x + 8 * cell, off_y + i * cell);
        p.drawLine(off_x + i * cell, off_y, off_x + i * cell, off_y + 8 * cell);
    }
}

void BoardWidget::DrawHighlights(QPainter& p) const {
    if (last_from_ >= 0 && last_to_ >= 0) {
        QColor last_col(246, 246, 105, 128);
        p.fillRect(SquareRect(last_from_), last_col);
        p.fillRect(SquareRect(last_to_), last_col);
    }
    if (selected_sq_.has_value()) {
        QColor sel_col(100, 149, 237, 128);
        p.fillRect(SquareRect(*selected_sq_), sel_col);
    }
    if (in_check_sq_ >= 0) {
        QColor check_col(255, 80, 80, 110);
        p.fillRect(SquareRect(in_check_sq_), check_col);
    }
    if (legal_mask_ != 0) {
        QColor dot_col(20, 20, 20, 160);

        for (int sq = 0; sq < 64; ++sq) {
            if ((legal_mask_ >> sq) & 1ULL) {
                const QRect r = SquareRect(sq);
                const int d = qMin(r.width(), r.height()) / 6;
                const int cx = r.center().x();
                const int cy = r.center().y();
                p.setBrush(dot_col);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(cx, cy), d, d);
            }
        }
    }
}

void BoardWidget::EnsurePiecePixmapsLoaded() const {
    if (!piece_pix_[0].isNull()) {
        return;
    }

    const char* keys[12] = {
        ":/img/source/img/pawn-white.png", ":/img/source/img/knight-white.png", ":/img/source/img/bishop-white.png",
        ":/img/source/img/rook-white.png", ":/img/source/img/queen-white.png", ":/img/source/img/king-white.png",
        ":/img/source/img/pawn-black.png", ":/img/source/img/knight-black.png", ":/img/source/img/bishop-black.png",
        ":/img/source/img/rook-black.png", ":/img/source/img/queen-black.png", ":/img/source/img/king-black.png"
    };

    for (int i = 0; i < 12; ++i) {
        piece_pix_[i] = QPixmap(keys[i]);
    }
}

void BoardWidget::DrawPieces(QPainter& p) const {
    EnsurePiecePixmapsLoaded();
    if (pieces_.size() != 64) {
        return;
    }

    for (int sq = 0; sq < 64; ++sq) {
        const unsigned char code = static_cast<unsigned char>(pieces_.at(sq));
        if (code == 0) {
            continue;
        }

        const QRect r = SquareRect(sq);
        const int idx0 = static_cast<int>(code) - 1; // 1..12 -> 0..11
        if (idx0 >= 0 && idx0 < 12 && !piece_pix_[idx0].isNull()) {
            p.drawPixmap(r, piece_pix_[idx0]);
        } else {
            p.setPen(Qt::black);
            QFont f = p.font();
            f.setPointSizeF(r.height() * 0.55);
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, QString::number(code));
        }
    }
}

void BoardWidget::DrawCoords(QPainter& p) const {
    const int w = width();
    const int h = height();
    const int size = qMin(w, h);
    const int off_x = (w - size) / 2;
    const int off_y = (h - size) / 2;
    const int cell = size / 8;

    QFont f = p.font();
    f.setPointSizeF(cell * 0.22);
    p.setFont(f);
    p.setPen(QColor(30, 30, 30));

    for (int i = 0; i < 8; ++i) {
        const int file_draw = white_bottom_ ? i : (7 - i);
        const int rank_draw = white_bottom_ ? (7 - i) : i;
        const QChar file_char('a' + file_draw);
        const QChar rank_char('1' + rank_draw);
        p.drawText(off_x + i * cell + 3, off_y + 8 * cell - 5, QString(file_char));
        p.drawText(off_x + 2, off_y + i * cell + 14, QString(rank_char));
    }
}

int BoardWidget::AskPromotionPiece(bool /*white_to_move*/) const {
    QStringList items;
    items << "Queen" << "Rook" << "Bishop" << "Knight";
    bool ok = false;
    const QString choice = QInputDialog::getItem(const_cast<BoardWidget*>(this),
                                                 "Promotion",
                                                 "Promote to:",
                                                 items, 0, false, &ok);

    if (!ok) {
        return 1;
    }
    if (choice == "Queen") {
        return 1;
    }
    if (choice == "Rook") {
        return 2;
    }
    if (choice == "Bishop") {
        return 3;
    }
    return 4;
}
