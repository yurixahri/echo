#include "popup.h"
#include <QWidget>
#include <QGridLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QEvent>
#include "utils/theme.h"

QPopup::QPopup(QWidget *parent, int gap_x, int gap_y)
    : QWidget{parent ? parent->window() : nullptr}
    , target_parent{parent}
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(get_popup_qss(theme_path));

    main_grid = new QGridLayout(this);
    main_grid->setContentsMargins(0, 0, 0, 0);
    panel = new QWidget(this);
    panel->setObjectName("popup_panel");
    panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    panel->setMinimumWidth(300);
    panel->setMaximumWidth(460);

    main_grid->addWidget(panel, 1, 1, Qt::AlignVCenter);

    set_gaps(gap_x, gap_y);

    if (this->window()) {
        this->window()->installEventFilter(this);
    }
    if (target_parent && target_parent != this->window()) {
        target_parent->installEventFilter(this);
    }
    reposition();

    apply_blur(5);
}

void QPopup::reposition() {
    if (target_parent && target_parent != this->window()) {
        QPoint top_left = target_parent->mapTo(this->window(), QPoint(0, 0));
        setGeometry(QRect(top_left, target_parent->size()));
    } else if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
}

QPopup::~QPopup() {
    if (this->window()) {
        this->window()->removeEventFilter(this);
    }
    if (target_parent) {
        target_parent->removeEventFilter(this);
        if (blur_effect && target_parent->graphicsEffect() == blur_effect) {
            target_parent->setGraphicsEffect(nullptr);
        }
    }
}

void QPopup::apply_blur(qreal radius) {
    if (!target_parent) return;

    blur_effect = new QGraphicsBlurEffect(target_parent);
    blur_effect->setBlurRadius(radius);
    blur_effect->setBlurHints(QGraphicsBlurEffect::QualityHint);
    target_parent->setGraphicsEffect(blur_effect);
}

void QPopup::set_panel_width(int min_w, int max_w) {
    if (panel) {
        panel->setMinimumWidth(min_w);
        panel->setMaximumWidth(max_w);
    }
}

void QPopup::set_gaps(int gap_x, int gap_y) {
    if (!main_grid) return;

    main_grid->setRowStretch(0, 1);
    main_grid->setRowStretch(1, 0);
    main_grid->setRowStretch(2, 1);

    int center_x = qMax(1, 100 - 2 * gap_x);
    main_grid->setColumnStretch(0, gap_x);
    main_grid->setColumnStretch(1, center_x);
    main_grid->setColumnStretch(2, gap_x);
}

bool QPopup::eventFilter(QObject *obj, QEvent *event) {
    if ((obj == target_parent || obj == parentWidget() || obj == this->window()) &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        reposition();
    }
    return QWidget::eventFilter(obj, event);
}

void QPopup::mousePressEvent(QMouseEvent *event) {
    if (!panel->geometry().contains(event->pos())) {
        this->deleteLater();
        return;
    }
    QWidget::mousePressEvent(event);
}


void QPopup::paintEvent(QPaintEvent *event){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw the dim background rectangle across the whole overlay space
    painter.fillRect(this->rect(),  QColor(0, 0, 0, 128));
}
