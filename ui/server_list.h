#ifndef SERVER_LIST_H
#define SERVER_LIST_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCoreApplication>
#include "ui/qcollapsewidget.h"
#include "utils/config.h"
#include "utils/theme.h"
#include "functions/user.h"

inline void update_server_list_ui(QCollapseWidget *server_list_widget) {
    if (!server_list_widget || !server_list_widget->content) return;

    QLayout *layout = server_list_widget->content->layout();
    if (!layout) {
        layout = new QVBoxLayout(server_list_widget->content);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(6);
    }

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QReadLocker lock(&config.lock);
    for (int i = 0; i < config.server_list.size(); ++i) {
        server_t server = config.server_list[i];
        QString address = server.address;

        QWidget *row = new QWidget();
        QHBoxLayout *row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(2, 1, 2, 1);
        row_layout->setSpacing(4);

        QPushButton *server_btn = new QPushButton(address);
        server_btn->setObjectName("server_item");
        server_btn->setCursor(Qt::PointingHandCursor);

        QObject::connect(server_btn, &QPushButton::clicked, [server]() {
            start_connection(server);
        });

        QPushButton *delete_btn = new QPushButton();
        delete_btn->setObjectName("delete_server");
        delete_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/close.svg",
                                              QColor(global_theme.foreground), QSize(12, 12))));
        delete_btn->setFixedSize(16, 16);
        delete_btn->setCursor(Qt::PointingHandCursor);

        QObject::connect(delete_btn, &QPushButton::clicked, [server_list_widget, address]() {
            {
                QWriteLocker wlock(&config.lock);
                for (int j = 0; j < config.server_list.size(); ++j) {
                    if (config.server_list[j].address == address) {
                        config.server_list.removeAt(j);
                        break;
                    }
                }
            }
            write_config_async();
            update_server_list_ui(server_list_widget);
        });

        row_layout->addWidget(server_btn, 1);
        row_layout->addWidget(delete_btn, 0);
        layout->addWidget(row);
    }
}

#endif // SERVER_LIST_H
