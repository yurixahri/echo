#ifndef ROOM_SETTINGS_POPUP_H
#define ROOM_SETTINGS_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>
#include "functions/room.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_room_settings_popup(QWidget *parent, const room_t &room, int gap_x = 22, int gap_y = 22) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Room Settings");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *name_lbl = new QLabel("Room Name");
    name_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(name_lbl);

    QLineEdit *name_input = new QLineEdit();
    name_input->setText(room.name);
    name_input->setPlaceholderText("Enter room name");
    name_input->setFixedHeight(32);
    layout->addWidget(name_input);

    QLabel *pwd_lbl = new QLabel("Room Password (leave empty to keep unchanged)");
    pwd_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(pwd_lbl);

    QLineEdit *pwd_input = new QLineEdit();
    pwd_input->setPlaceholderText("Enter new password (or blank to keep current)");
    pwd_input->setEchoMode(QLineEdit::Password);
    pwd_input->setFixedHeight(32);
    layout->addWidget(pwd_input);

    QCheckBox *clear_pwd_cb = nullptr;
    if (room.has_password) {
        clear_pwd_cb = new QCheckBox("Remove password (make room public)");
        layout->addWidget(clear_pwd_cb);
        QObject::connect(clear_pwd_cb, &QCheckBox::toggled, [pwd_input](bool checked) {
            pwd_input->setEnabled(!checked);
            if (checked) pwd_input->clear();
        });
    }

    QHBoxLayout *power_layout = new QHBoxLayout();
    power_layout->setSpacing(6);

    auto make_power_input = [](const QString &lbl_text, int val, QLineEdit **out_edit, QLayout *parent_lay) {
        QVBoxLayout *col = new QVBoxLayout();
        col->setSpacing(2);
        QLabel *lbl = new QLabel(lbl_text);
        lbl->setObjectName("popup_muted_lbl");
        QLineEdit *ed = new QLineEdit(QString::number(val));
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

    make_power_input("Req Voice Join", room.needed_voice_join_power, &voice_p_in, power_layout);
    make_power_input("Req Chat Join", room.needed_chat_join_power, &chat_p_in, power_layout);
    make_power_input("Req Speak Power", room.needed_speak_power, &speak_p_in, power_layout);

    layout->addLayout(power_layout);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(30);

    QPushButton *save_btn = new QPushButton("Save");
    save_btn->setObjectName("popup_primary_btn");
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedHeight(30);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(save_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    uint64_t target_room_id = room.id;
    auto submit_update = [target_room_id, name_input, pwd_input, clear_pwd_cb, voice_p_in, chat_p_in, speak_p_in, popup]() {
        QString name = name_input->text().trimmed();
        if (name.isEmpty()) return;
        QString pwd = pwd_input->text();
        if (clear_pwd_cb && clear_pwd_cb->isChecked()) {
            pwd = "__CLEAR__";
            client.server.room_passwords.remove(target_room_id);
            write_config_async();
        } else if (!pwd.isEmpty()) {
            client.server.room_passwords.remove(target_room_id);
            write_config_async();
        }
        uint v_power = voice_p_in ? voice_p_in->text().toUInt() : 0;
        uint c_power = chat_p_in ? chat_p_in->text().toUInt() : 0;
        uint s_power = speak_p_in ? speak_p_in->text().toUInt() : 0;

        user_update_room(target_room_id, name, pwd, v_power, c_power, s_power);
        popup->deleteLater();
    };

    QObject::connect(cancel_btn, &QPushButton::clicked, popup, &QObject::deleteLater);
    QObject::connect(save_btn, &QPushButton::clicked, popup, submit_update);
    QObject::connect(name_input, &QLineEdit::returnPressed, popup, submit_update);
    QObject::connect(pwd_input, &QLineEdit::returnPressed, popup, submit_update);

    popup->show();
    return popup;
}

#endif // ROOM_SETTINGS_POPUP_H
