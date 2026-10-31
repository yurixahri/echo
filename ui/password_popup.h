#ifndef PASSWORD_POPUP_H
#define PASSWORD_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "ui/popup.h"
#include "utils/theme.h"
#include "utils/config.h"
#include "functions/user.h"
#include "functions/room.h"

inline QPopup* open_server_password_popup(QWidget *parent, const QString &err_msg = "") {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(8);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Server Password");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    if (!err_msg.isEmpty()) {
        QLabel *err_label = new QLabel(err_msg);
        err_label->setObjectName("popup_err_lbl");
        err_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(err_label);
    } else {
        QLabel *desc_label = new QLabel("This server requires a password to enter.");
        desc_label->setObjectName("popup_desc");
        desc_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(desc_label);
    }

    QLineEdit *pwd_input = new QLineEdit();
    pwd_input->setPlaceholderText("Enter server password");
    pwd_input->setEchoMode(QLineEdit::Password);
    pwd_input->setFixedHeight(32);
    layout->addWidget(pwd_input);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(30);

    QPushButton *connect_btn = new QPushButton("Connect");
    connect_btn->setObjectName("popup_primary_btn");
    connect_btn->setCursor(Qt::PointingHandCursor);
    connect_btn->setFixedHeight(30);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(connect_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, 25, 30);
    popup->panel->setLayout(layout);

    auto submit_action = [pwd_input, popup]() {
        QString pwd = pwd_input->text();
        client.server.password = pwd;

        {
            QWriteLocker lock(&config.lock);
            for (server_t &s : config.server_list) {
                if (s.address == client.server.address) {
                    s.password = pwd;
                    break;
                }
            }
        }
        write_config_async();
        popup->deleteLater();

        start_connection(client.server);
    };

    QObject::connect(connect_btn, &QPushButton::clicked, popup, submit_action);
    QObject::connect(pwd_input, &QLineEdit::returnPressed, popup, submit_action);
    QObject::connect(cancel_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    pwd_input->setFocus();
    return popup;
}

inline QPopup* open_room_password_popup(QWidget *parent, uint64_t room_id, const QString &room_name, int join_type, const QString &err_msg = "") {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(8);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Room Password");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    if (!err_msg.isEmpty()) {
        QLabel *err_label = new QLabel(err_msg);
        err_label->setObjectName("popup_err_lbl");
        err_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(err_label);
    } else {
        QLabel *desc_label = new QLabel("Enter password for: " + room_name);
        desc_label->setObjectName("popup_desc");
        desc_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(desc_label);
    }

    QLineEdit *pwd_input = new QLineEdit();
    pwd_input->setPlaceholderText("Enter room password");
    pwd_input->setEchoMode(QLineEdit::Password);
    pwd_input->setFixedHeight(32);
    layout->addWidget(pwd_input);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(30);

    QPushButton *join_btn = new QPushButton("Join");
    join_btn->setObjectName("popup_primary_btn");
    join_btn->setCursor(Qt::PointingHandCursor);
    join_btn->setFixedHeight(30);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(join_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, 25, 30);
    popup->panel->setLayout(layout);

    auto submit_action = [pwd_input, room_id, join_type, popup]() {
        QString pwd = pwd_input->text();
        client.server.room_passwords[room_id] = pwd;

        {
            QWriteLocker lock(&config.lock);
            for (server_t &s : config.server_list) {
                if (s.address == client.server.address) {
                    s.room_passwords[room_id] = pwd;
                    break;
                }
            }
        }
        write_config_async();
        popup->deleteLater();

        user_join_room(room_id, pwd, join_type);
    };

    QObject::connect(join_btn, &QPushButton::clicked, popup, submit_action);
    QObject::connect(pwd_input, &QLineEdit::returnPressed, popup, submit_action);
    QObject::connect(cancel_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    pwd_input->setFocus();
    return popup;
}

#endif // PASSWORD_POPUP_H
