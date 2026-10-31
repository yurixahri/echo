#ifndef TOAST_H
#define TOAST_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPointer>
#include <QEvent>
#include "utils/theme.h"

class ToastContainer : public QWidget {
    Q_OBJECT
public:
    explicit ToastContainer(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("toast_container");
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(get_toast_qss(theme_path));

        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(6);
        lay->setSizeConstraint(QLayout::SetFixedSize);
        lay->setAlignment(Qt::AlignTop | Qt::AlignRight);

        if (parent) {
            parent->installEventFilter(this);
            reposition();
        }
        hide();
    }

    void reposition() {
        if (!parentWidget()) return;
        int x = parentWidget()->width() - width() - 16;
        int y = 40;
        move(x, y);
        raise();
    }

    void remove_toast(QWidget *card) {
        if (!card) return;
        QVBoxLayout *lay = qobject_cast<QVBoxLayout*>(layout());
        if (lay) {
            lay->removeWidget(card);
        }
        card->hide();
        card->deleteLater();

        adjustSize();
        reposition();

        if (!lay || lay->count() == 0) {
            hide();
        }
    }

    void add_toast(const QString &message, const QString &type = "info", int timeout_ms = 3500) {
        constexpr int max_toasts = 3;
        QVBoxLayout *lay = qobject_cast<QVBoxLayout*>(layout());
        if (!lay) return;

        while (lay->count() >= max_toasts) {
            QLayoutItem *item = lay->takeAt(0);
            if (item) {
                if (item->widget()) {
                    item->widget()->hide();
                    item->widget()->deleteLater();
                }
                delete item;
            }
        }

        QWidget *card = new QWidget(this);
        card->setObjectName("toast_card_" + type);
        card->setProperty("toast_type", type);
        card->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setMinimumWidth(240);
        card->setMaximumWidth(360);
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

        QString icon_char = "ℹ";
        QPixmap custom_icon;

        if (type == "error") {
            custom_icon = colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/warning.svg",
                                      QColor(global_theme.destructive), QSize(16, 16));
        } else if (type == "success") {
            icon_char = "✓";
        }

        card->setProperty("toast_type", type);

        QHBoxLayout *card_lay = new QHBoxLayout(card);
        card_lay->setContentsMargins(10, 8, 10, 8);
        card_lay->setSpacing(8);

        QLabel *icon_lbl = new QLabel(card);
        icon_lbl->setObjectName("toast_icon");
        icon_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        if (!custom_icon.isNull()) {
            icon_lbl->setPixmap(custom_icon);
            icon_lbl->setFixedSize(16, 16);
        } else {
            icon_lbl->setText(icon_char);
        }

        QLabel *text_lbl = new QLabel(message, card);
        text_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        text_lbl->setWordWrap(true);

        card_lay->addWidget(icon_lbl, 0, Qt::AlignTop);
        card_lay->addWidget(text_lbl, 1, Qt::AlignVCenter);

        lay->addWidget(card);
        adjustSize();
        reposition();
        show();
        raise();

        QPointer<ToastContainer> self(this);
        QPointer<QWidget> card_ptr(card);
        QTimer::singleShot(timeout_ms, [self, card_ptr]() {
            if (self && card_ptr) {
                self->remove_toast(card_ptr.data());
            }
        });
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj == parentWidget() && (ev->type() == QEvent::Resize || ev->type() == QEvent::Move)) {
            reposition();
        }
        return QWidget::eventFilter(obj, ev);
    }
};

inline ToastContainer* get_or_create_toast_container(QWidget *parent) {
    if (!parent) return nullptr;
    QWidget *top = parent->window();
    if (!top) top = parent;

    ToastContainer *tc = top->findChild<ToastContainer*>("toast_container");
    if (!tc) {
        tc = new ToastContainer(top);
    }
    return tc;
}

inline void show_toast(QWidget *parent, const QString &message, const QString &type = "info", int timeout_ms = 3500) {
    ToastContainer *tc = get_or_create_toast_container(parent);
    if (tc) {
        tc->add_toast(message, type, timeout_ms);
    }
}

#endif // TOAST_H
