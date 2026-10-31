#ifndef CLAIM_ADMIN_H
#define CLAIM_ADMIN_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "functions/user.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_claim_admin_popup(QWidget *parent, int gap_x = 22, int gap_y = 30) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(12);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Claim Admin Privilege");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *desc_label = new QLabel("Enter the server initial setup token to claim administrator privileges.");
    desc_label->setObjectName("popup_desc");
    desc_label->setWordWrap(true);
    desc_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(desc_label);

    QLineEdit *token_input = new QLineEdit();
    token_input->setPlaceholderText("Enter admin token");
    token_input->setFixedHeight(32);
    layout->addWidget(token_input);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *close_btn = new QPushButton("Cancel");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(30);

    QPushButton *claim_btn = new QPushButton("Claim");
    claim_btn->setObjectName("popup_primary_btn");
    claim_btn->setCursor(Qt::PointingHandCursor);
    claim_btn->setFixedHeight(30);

    btn_layout->addWidget(close_btn);
    btn_layout->addWidget(claim_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    auto submit_action = [token_input, popup]() {
        QString token = token_input->text().trimmed();
        if (!token.isEmpty()) {
            user_claim_admin_privilege(token);
            popup->deleteLater();
        }
    };

    QObject::connect(claim_btn, &QPushButton::clicked, popup, submit_action);
    QObject::connect(token_input, &QLineEdit::returnPressed, popup, submit_action);
    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    return popup;
}

#endif // CLAIM_ADMIN_H
