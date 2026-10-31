#ifndef SERVER_HEADER_H
#define SERVER_HEADER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include "ui/claim_admin.h"
#include "ui/create_room.h"
#include "ui/server_settings.h"
#include "ui/server_users_popup.h"
#include "ui/my_info.h"
#include "functions/user.h"
#include "functions/voice.h"
#include "utils/theme.h"

struct server_header_t {
    QWidget *widget = nullptr;
    QLabel *server_name = nullptr;
    QPushButton *disconnect_btn = nullptr;
    QPushButton *my_info_btn = nullptr;
    QPushButton *user_list_btn = nullptr;
    QPushButton *menu_btn = nullptr;
    QMenu *menu = nullptr;
    QAction *claim_admin_action = nullptr;
    QAction *create_room_action = nullptr;
    QAction *server_settings_action = nullptr;
};

inline server_header_t create_server_header(QWidget *parent, QWidget *popup_anchor) {
    server_header_t header;
    header.widget = new QWidget(parent);
    header.widget->setObjectName("server_header");

    QHBoxLayout *layout = new QHBoxLayout(header.widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    header.server_name = new QLabel("Not connected", header.widget);
    header.server_name->setObjectName("server_name_title");

    header.disconnect_btn = new QPushButton(header.widget);
    header.disconnect_btn->setObjectName("server_menu_btn");
    header.disconnect_btn->setCursor(Qt::PointingHandCursor);
    header.disconnect_btn->setFixedSize(28, 28);
    header.disconnect_btn->setToolTip("Disconnect");

    header.my_info_btn = new QPushButton(header.widget);
    header.my_info_btn->setObjectName("server_menu_btn");
    header.my_info_btn->setCursor(Qt::PointingHandCursor);
    header.my_info_btn->setFixedSize(28, 28);
    header.my_info_btn->setToolTip("My Profile");

    header.user_list_btn = new QPushButton(header.widget);
    header.user_list_btn->setObjectName("server_menu_btn");
    header.user_list_btn->setCursor(Qt::PointingHandCursor);
    header.user_list_btn->setFixedSize(28, 28);
    header.user_list_btn->setToolTip("Server Users");

    header.menu_btn = new QPushButton("⋯", header.widget);
    header.menu_btn->setObjectName("server_menu_btn");
    header.menu_btn->setCursor(Qt::PointingHandCursor);
    header.menu_btn->setFixedSize(28, 28);
    header.menu_btn->setToolTip("Server Menu");

    header.menu = new QMenu(header.widget);
    header.menu->setObjectName("server_context_menu");

    // 1. Claim Admin
    QIcon claim_admin_icon;
    header.claim_admin_action = header.menu->addAction(claim_admin_icon, "Claim Admin");

    // 2. Create Room
    QIcon create_room_icon;
    header.create_room_action = header.menu->addAction(create_room_icon, "Create Room");

    header.menu->addSeparator();

    // 3. Server Settings
    header.server_settings_action = header.menu->addAction("Server Settings");

    // Actions triggering popups
    QObject::connect(header.claim_admin_action, &QAction::triggered, [popup_anchor]() {
        open_claim_admin_popup(popup_anchor);
    });

    QObject::connect(header.create_room_action, &QAction::triggered, [popup_anchor]() {
        open_create_room_popup(popup_anchor);
    });

    QObject::connect(header.server_settings_action, &QAction::triggered, [popup_anchor]() {
        open_server_settings_popup(popup_anchor);
    });

    QObject::connect(header.disconnect_btn, &QPushButton::clicked, []() {
        is_manual_disconnect = true;
        desired_voice_room_id = 0;
        if (reconnect_timer) reconnect_timer->stop();
        stop_voice_channel(true);
        if (client.network_worker) {
            ClientNetworkWorker *worker = client.network_worker;
            bool queued = QMetaObject::invokeMethod(
                worker, [worker]() { worker->disconnect_from_host(); },
                Qt::QueuedConnection);
            if (!queued) qWarning() << "Could not queue server disconnect.";
        }
    });

    QObject::connect(header.my_info_btn, &QPushButton::clicked, [popup_anchor]() {
        open_my_info_popup(popup_anchor);
    });

    QObject::connect(header.user_list_btn, &QPushButton::clicked, [popup_anchor]() {
        open_server_users_popup(popup_anchor);
    });

    QPushButton *btn = header.menu_btn;
    QMenu *menu = header.menu;
    QObject::connect(header.menu_btn, &QPushButton::clicked, [btn, menu]() {
        menu->exec(btn->mapToGlobal(QPoint(btn->width() - menu->sizeHint().width(), btn->height() + 4)));
    });

    layout->addWidget(header.server_name, 1);
    layout->addWidget(header.disconnect_btn, 0);
    layout->addWidget(header.my_info_btn, 0);
    layout->addWidget(header.user_list_btn, 0);
    layout->addWidget(header.menu_btn, 0);

    return header;
}

inline void update_server_header_icons(server_header_t &header) {
    QString icons_dir = QCoreApplication::applicationDirPath() + "/assets/icons/";
    if (header.disconnect_btn) {
        header.disconnect_btn->setIcon(QIcon(colored_svg(icons_dir + "exit.svg", QColor(global_theme.destructive), QSize(16, 16))));
    }
    if (header.my_info_btn) {
        header.my_info_btn->setIcon(QIcon(colored_svg(icons_dir + "user_info.svg", QColor(global_theme.foreground), QSize(16, 16))));
    }
    if (header.user_list_btn) {
        header.user_list_btn->setIcon(QIcon(colored_svg(icons_dir + "user_list.svg", QColor(global_theme.foreground), QSize(16, 16))));
    }
}

#endif // SERVER_HEADER_H
