#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "functions/room.h"
#include "ui/popup.h"
#include "ui/roles_popup.h"
#include "ui/server_users_popup.h"
#include "utils/theme.h"

inline QPopup* open_server_settings_popup(QWidget *parent, int gap_x = 20, int gap_y = 25) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(12);
    layout->setContentsMargins(15, 15, 15, 15);

    bool is_admin = client.is_admin;

    QLabel *title_label = new QLabel("Server Settings");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    if (!is_admin) {
        QLabel *warn_lbl = new QLabel("Administrator privileges required to edit settings.");
        warn_lbl->setObjectName("popup_err_lbl");
        warn_lbl->setAlignment(Qt::AlignCenter);
        layout->addWidget(warn_lbl);
    }

    QLabel *name_label = new QLabel("Server Name");
    name_label->setObjectName("popup_muted_lbl");
    layout->addWidget(name_label);

    QLineEdit *name_input = new QLineEdit();
    name_input->setPlaceholderText("Enter server name");
    name_input->setText(client.server_name.isEmpty() ? client.server.address : client.server_name);
    name_input->setFixedHeight(32);
    name_input->setEnabled(is_admin);
    layout->addWidget(name_input);

    QLabel *pwd_label = new QLabel("Server Password (leave empty to keep unchanged)");
    pwd_label->setObjectName("popup_muted_lbl");
    layout->addWidget(pwd_label);

    QLineEdit *pwd_input = new QLineEdit();
    pwd_input->setPlaceholderText("Enter new server password");
    pwd_input->setEchoMode(QLineEdit::Password);
    pwd_input->setFixedHeight(32);
    pwd_input->setEnabled(is_admin);
    layout->addWidget(pwd_input);

    QCheckBox *clear_pwd_cb = new QCheckBox("Remove server password (make public)");
    clear_pwd_cb->setEnabled(is_admin);
    layout->addWidget(clear_pwd_cb);
    QObject::connect(clear_pwd_cb, &QCheckBox::toggled, [pwd_input, is_admin](bool checked) {
        if (!is_admin) return;
        pwd_input->setEnabled(!checked);
        if (checked) pwd_input->clear();
    });

    QLabel *roles_header = new QLabel("Roles & Permissions");
    roles_header->setObjectName("popup_section_title");
    layout->addWidget(roles_header);

    QPushButton *manage_roles_btn = new QPushButton("Manage Roles & Permissions...");
    manage_roles_btn->setObjectName("popup_secondary_btn");
    manage_roles_btn->setCursor(Qt::PointingHandCursor);
    manage_roles_btn->setFixedHeight(32);
    manage_roles_btn->setEnabled(is_admin);
    layout->addWidget(manage_roles_btn);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(30);

    QPushButton *save_btn = new QPushButton("Save");
    save_btn->setObjectName("popup_primary_btn");
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedHeight(30);
    save_btn->setEnabled(is_admin);

    btn_layout->addWidget(close_btn);
    btn_layout->addWidget(save_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    auto submit_settings = [name_input, pwd_input, clear_pwd_cb, is_admin, popup]() {
        if (!is_admin) {
            popup->deleteLater();
            return;
        }
        QString name = name_input->text().trimmed();
        QString pwd = pwd_input->text();
        if (clear_pwd_cb && clear_pwd_cb->isChecked()) {
            pwd = "__CLEAR__";
            client.server.password.clear();
            {
                QWriteLocker lock(&config.lock);
                for (server_t &s : config.server_list) {
                    if (s.address == client.server.address) {
                        s.password.clear();
                        break;
                    }
                }
            }
            write_config_async();
        } else if (!pwd.isEmpty()) {
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
        }

        user_update_server_settings(name, pwd);
        popup->deleteLater();
    };

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);
    QObject::connect(save_btn, &QPushButton::clicked, popup, submit_settings);
    QObject::connect(pwd_input, &QLineEdit::returnPressed, popup, submit_settings);
    QObject::connect(name_input, &QLineEdit::returnPressed, popup, submit_settings);
    QObject::connect(manage_roles_btn, &QPushButton::clicked, [parent]() {
        open_roles_popup(parent);
    });

    popup->raise();
    popup->show();
    return popup;
}

#endif // SERVER_SETTINGS_H
