#ifndef CONNECTION_POPUP_H
#define CONNECTION_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include "ui/popup.h"
#include "utils/theme.h"
#include "functions/user.h"

inline QPopup* show_loading_popup(QWidget *parent, const QString &text = "Connecting to server...", int gap_x = 25, int gap_y = 35) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(12);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *msg_label = new QLabel(text);
    msg_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(msg_label);

    QProgressBar *bar = new QProgressBar();
    bar->setRange(0, 0); // Indeterminate animation mode
    bar->setTextVisible(false);
    bar->setFixedHeight(4);
    layout->addWidget(bar);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(28);
    layout->addWidget(cancel_btn);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    QObject::connect(cancel_btn, &QPushButton::clicked, popup, [popup]() {
        asn1_client_stop_network();
        client.state = FAILED;
        popup->deleteLater();
    });

    popup->raise();
    popup->show();
    return popup;
}

inline QPopup* show_error_popup(QWidget *parent, const QString &error_text, int gap_x = 25, int gap_y = 35) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(12);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Connection Error");
    title_label->setObjectName("popup_err_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *msg_label = new QLabel(error_text);
    msg_label->setWordWrap(true);
    msg_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(msg_label);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(28);
    layout->addWidget(close_btn);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    return popup;
}

#endif // CONNECTION_POPUP_H
