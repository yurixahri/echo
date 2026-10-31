#ifndef ROLES_POPUP_H
#define ROLES_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "functions/role.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QWidget* create_role_editor_card(const role_t &role, QWidget *parent, std::function<void()> on_refresh) {
    QWidget *card = new QWidget(parent);
    card->setObjectName("role_card");
    QVBoxLayout *card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(10, 8, 10, 8);
    card_layout->setSpacing(6);

    // Header row
    QWidget *header = new QWidget(card);
    QHBoxLayout *header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    QLabel *name_lbl = new QLabel(role.name, header);
    name_lbl->setObjectName("role_name_lbl");

    QPushButton *toggle_btn = new QPushButton("Edit Permissions", header);
    toggle_btn->setObjectName("role_toggle_btn");
    toggle_btn->setCursor(Qt::PointingHandCursor);
    toggle_btn->setFixedHeight(24);

    QPushButton *del_btn = new QPushButton("Delete", header);
    del_btn->setObjectName("role_del_btn");
    del_btn->setCursor(Qt::PointingHandCursor);
    del_btn->setFixedHeight(24);

    header_layout->addWidget(name_lbl, 1);
    header_layout->addWidget(toggle_btn, 0);
    header_layout->addWidget(del_btn, 0);
    card_layout->addWidget(header);

    // Permissions editor panel (hidden by default)
    QWidget *editor = new QWidget(card);
    editor->setObjectName("role_editor_panel");
    editor->hide();
    QVBoxLayout *edit_layout = new QVBoxLayout(editor);
    edit_layout->setContentsMargins(0, 6, 0, 0);
    edit_layout->setSpacing(6);

    QLabel *can_title = new QLabel("General Permissions (Yes/No):", editor);
    can_title->setObjectName("role_section_title");
    edit_layout->addWidget(can_title);

    QCheckBox *cb_create = new QCheckBox("Can create rooms (u_can_create_rooms)", editor);
    cb_create->setChecked(role.permissions.value("u_can_create_rooms", 0) > 0);
    edit_layout->addWidget(cb_create);

    QCheckBox *cb_del = new QCheckBox("Can delete rooms (u_can_delete_rooms)", editor);
    cb_del->setChecked(role.permissions.value("u_can_delete_rooms", 0) > 0);
    edit_layout->addWidget(cb_del);

    QCheckBox *cb_ban = new QCheckBox("Can ban users (u_can_ban_users)", editor);
    cb_ban->setChecked(role.permissions.value("u_can_ban_users", 0) > 0);
    edit_layout->addWidget(cb_ban);

    QCheckBox *cb_kick = new QCheckBox("Can kick users (u_can_kick_users)", editor);
    cb_kick->setChecked(role.permissions.value("u_can_kick_users", 0) > 0);
    edit_layout->addWidget(cb_kick);

    QLabel *power_title = new QLabel("Power Levels (0 - 999999):", editor);
    power_title->setObjectName("role_section_title");
    edit_layout->addWidget(power_title);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(6);

    auto make_spin = [](int val) {
        QSpinBox *sb = new QSpinBox();
        sb->setRange(0, 999999);
        sb->setValue(val);
        sb->setFixedHeight(28);
        return sb;
    };

    QSpinBox *sb_voice = make_spin(role.permissions.value("u_voice_join_power", 0));
    QSpinBox *sb_chat = make_spin(role.permissions.value("u_chat_join_power", 0));
    QSpinBox *sb_speak = make_spin(role.permissions.value("u_speak_power", 0));

    grid->addWidget(new QLabel("Voice Join Power:"), 0, 0);
    grid->addWidget(sb_voice, 0, 1);
    grid->addWidget(new QLabel("Chat Join Power:"), 1, 0);
    grid->addWidget(sb_chat, 1, 1);
    grid->addWidget(new QLabel("Speak Power:"), 2, 0);
    grid->addWidget(sb_speak, 2, 1);
    edit_layout->addLayout(grid);

    QPushButton *save_perms_btn = new QPushButton("Save Permissions", editor);
    save_perms_btn->setObjectName("role_save_perms_btn");
    save_perms_btn->setCursor(Qt::PointingHandCursor);
    save_perms_btn->setFixedHeight(28);
    edit_layout->addWidget(save_perms_btn);

    card_layout->addWidget(editor);

    QObject::connect(toggle_btn, &QPushButton::clicked, [editor, toggle_btn]() {
        bool visible = !editor->isVisible();
        editor->setVisible(visible);
        toggle_btn->setText(visible ? "Collapse" : "Edit Permissions");
    });

    uint64_t rid = role.id;
    QObject::connect(del_btn, &QPushButton::clicked, [rid, on_refresh]() {
        user_delete_role(rid);
        if (on_refresh) on_refresh();
    });

    QObject::connect(save_perms_btn, &QPushButton::clicked, [rid, cb_create, cb_del, cb_ban, cb_kick, sb_voice, sb_chat, sb_speak, save_perms_btn]() {
        QJsonObject obj;
        obj["u_can_create_rooms"] = cb_create->isChecked() ? 1 : 0;
        obj["u_can_delete_rooms"] = cb_del->isChecked() ? 1 : 0;
        obj["u_can_ban_users"] = cb_ban->isChecked() ? 1 : 0;
        obj["u_can_kick_users"] = cb_kick->isChecked() ? 1 : 0;
        obj["u_voice_join_power"] = sb_voice->value();
        obj["u_chat_join_power"] = sb_chat->value();
        obj["u_speak_power"] = sb_speak->value();

        QString json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        user_update_role_permissions(rid, json);
        save_perms_btn->setText("Saved!");
        QTimer::singleShot(1000, [save_perms_btn]() {
            save_perms_btn->setText("Save Permissions");
        });
    });

    return card;
}

inline QPopup* open_roles_popup(QWidget *parent, int gap_x = 20, int gap_y = 20) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Manage Server Roles");
    title_label->setObjectName("roles_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    // Add role row
    QWidget *create_row = new QWidget();
    QHBoxLayout *c_lay = new QHBoxLayout(create_row);
    c_lay->setContentsMargins(0, 0, 0, 0);
    c_lay->setSpacing(6);

    QLineEdit *role_name_input = new QLineEdit(create_row);
    role_name_input->setObjectName("role_name_input");
    role_name_input->setPlaceholderText("Enter new role name");
    role_name_input->setFixedHeight(32);

    QPushButton *create_btn = new QPushButton("Create Role", create_row);
    create_btn->setObjectName("role_create_btn");
    create_btn->setCursor(Qt::PointingHandCursor);
    create_btn->setFixedHeight(32);

    c_lay->addWidget(role_name_input, 1);
    c_lay->addWidget(create_btn, 0);
    layout->addWidget(create_row);

    // Roles scrollable list
    QScrollArea *scroll = new QScrollArea();
    scroll->setObjectName("roles_scroll_area");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(240);
    scroll->setMaximumHeight(380);

    QWidget *roles_container = new QWidget();
    QVBoxLayout *roles_layout = new QVBoxLayout(roles_container);
    roles_layout->setContentsMargins(2, 2, 2, 2);
    roles_layout->setSpacing(8);
    roles_layout->setAlignment(Qt::AlignTop);

    QLabel *loading_lbl = new QLabel("Loading roles...");
    loading_lbl->setObjectName("role_section_title");
    roles_layout->addWidget(loading_lbl);

    scroll->setWidget(roles_container);
    layout->addWidget(scroll);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("roles_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(30);
    layout->addWidget(close_btn);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->setProperty("is_roles_popup", true);
    popup->setStyleSheet(get_roles_popup_qss(theme_path));
    popup->set_panel_width(440, 560);
    popup->panel->setLayout(layout);

    QPointer<QWidget> container_ptr(roles_container);
    auto refresh_roles = []() {
        user_request_roles();
    };

    on_roles_received = [container_ptr, refresh_roles](const QList<role_t> &roles) {
        if (!container_ptr) return;
        QVBoxLayout *lay = qobject_cast<QVBoxLayout*>(container_ptr->layout());
        if (!lay) return;

        while (QLayoutItem *it = lay->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }

        if (roles.isEmpty()) {
            QLabel *none_lbl = new QLabel("No roles configured on this server.", container_ptr);
            none_lbl->setObjectName("role_section_title");
            lay->addWidget(none_lbl);
            return;
        }

        for (const role_t &r : roles) {
            QWidget *card = create_role_editor_card(r, container_ptr, refresh_roles);
            lay->addWidget(card);
        }
    };

    QObject::connect(create_btn, &QPushButton::clicked, [role_name_input, refresh_roles]() {
        QString name = role_name_input->text().trimmed();
        if (!name.isEmpty()) {
            user_create_role(name);
            role_name_input->clear();
            QTimer::singleShot(200, refresh_roles);
        }
    });

    QObject::connect(role_name_input, &QLineEdit::returnPressed, [create_btn]() {
        create_btn->click();
    });

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();

    refresh_roles();
    return popup;
}

#endif // ROLES_POPUP_H
