#ifndef CREATE_ROOM_H
#define CREATE_ROOM_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>
#include "functions/room.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_create_room_popup(QWidget *parent, int gap_x = 22, int gap_y = 22) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Create Voice Room");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *name_lbl = new QLabel("Room Name");
    name_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(name_lbl);

    QLineEdit *name_input = new QLineEdit();
    name_input->setPlaceholderText("Enter room name");
    name_input->setFixedHeight(32);
    layout->addWidget(name_input);

    QLabel *pwd_lbl = new QLabel("Room Password (Optional)");
    pwd_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(pwd_lbl);

    QLineEdit *pwd_input = new QLineEdit();
    pwd_input->setPlaceholderText("Leave blank for public room");
    pwd_input->setEchoMode(QLineEdit::Password);
    pwd_input->setFixedHeight(32);
    layout->addWidget(pwd_input);

    QLabel *limit_lbl = new QLabel("Max Clients (0 for unlimited)");
    limit_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(limit_lbl);

    QLineEdit *limit_input = new QLineEdit();
    limit_input->setText("0");
    limit_input->setValidator(new QIntValidator(0, 999, limit_input));
    limit_input->setFixedHeight(32);
    layout->addWidget(limit_input);

    QHBoxLayout *power_layout = new QHBoxLayout();
    power_layout->setSpacing(6);

    auto make_power_input = [](const QString &lbl_text, QLineEdit **out_edit, QLayout *parent_lay) {
        QVBoxLayout *col = new QVBoxLayout();
        col->setSpacing(2);
        QLabel *lbl = new QLabel(lbl_text);
        lbl->setObjectName("popup_muted_lbl");
        QLineEdit *ed = new QLineEdit("0");
        ed->setValidator(new QIntValidator(0, 999999, ed));
        ed->setFixedHeight(28);
        col->addWidget(lbl);
        col->addWidget(ed);
        parent_lay->addItem(col);
        *out_edit = ed;
    };

    QLineEdit *voice_p_in = nullptr;
    QLineEdit *chat_p_in = nullptr;
    QLineEdit *speak_p_in = nullptr;

    make_power_input("Req Voice Join", &voice_p_in, power_layout);
    make_power_input("Req Chat Join", &chat_p_in, power_layout);
    make_power_input("Req Speak Power", &speak_p_in, power_layout);

    layout->addLayout(power_layout);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(30);

    QPushButton *create_btn = new QPushButton("Create");
    create_btn->setObjectName("popup_primary_btn");
    create_btn->setCursor(Qt::PointingHandCursor);
    create_btn->setFixedHeight(30);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(create_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    auto submit_create = [name_input, pwd_input, limit_input, voice_p_in, chat_p_in, speak_p_in, popup]() {
        QString room_name = name_input->text().trimmed();
        if (room_name.isEmpty()) {
            name_input->setFocus();
            return;
        }

        QString room_pwd = pwd_input->text();
        int client_limit = limit_input->text().toInt();
        uint vp = voice_p_in ? voice_p_in->text().toUInt() : 0;
        uint cp = chat_p_in ? chat_p_in->text().toUInt() : 0;
        uint sp = speak_p_in ? speak_p_in->text().toUInt() : 0;

        user_create_room(room_name, room_pwd, client_limit, vp, cp, sp);
        popup->deleteLater();
    };

    QObject::connect(create_btn, &QPushButton::clicked, popup, submit_create);
    QObject::connect(name_input, &QLineEdit::returnPressed, popup, submit_create);
    QObject::connect(cancel_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    name_input->setFocus();
    return popup;
}

#endif // CREATE_ROOM_H
