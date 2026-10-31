#ifndef SERVER_USERS_POPUP_H
#define SERVER_USERS_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QPointer>
#include <QMenu>
#include "functions/user.h"
#include "functions/file.h"
#include "ui/popup.h"
#include "ui/user_info_popup.h"
#include "ui/user_roles_popup.h"
#include "utils/theme.h"

inline QPopup* open_server_users_popup(QWidget *parent, int gap_x = 22, int gap_y = 22) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Server Users");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QScrollArea *scroll = new QScrollArea();
    scroll->setObjectName("roles_scroll_area");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(240);
    scroll->setMaximumHeight(380);

    QWidget *users_container = new QWidget();
    QVBoxLayout *users_layout = new QVBoxLayout(users_container);
    users_layout->setContentsMargins(2, 2, 2, 2);
    users_layout->setSpacing(6);
    users_layout->setAlignment(Qt::AlignTop);

    QLabel *loading_lbl = new QLabel("Loading members...");
    loading_lbl->setObjectName("popup_muted_lbl");
    users_layout->addWidget(loading_lbl);

    scroll->setWidget(users_container);
    layout->addWidget(scroll);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(30);
    layout->addWidget(close_btn);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->setStyleSheet(get_popup_qss(theme_path));
    popup->set_panel_width(400, 520);
    popup->panel->setLayout(layout);

    QPointer<QWidget> container_ptr(users_container);
    QString default_avt_path = QCoreApplication::applicationDirPath() + "/assets/defaults/user.png";
    QPixmap def_p(default_avt_path);

    on_member_list_received = [container_ptr, parent, def_p](const QList<member_item_t> &members) {
        if (!container_ptr) return;
        QVBoxLayout *lay = qobject_cast<QVBoxLayout*>(container_ptr->layout());
        if (!lay) return;

        while (QLayoutItem *it = lay->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }

        if (members.isEmpty()) {
            QLabel *none_lbl = new QLabel("No members found.", container_ptr);
            none_lbl->setObjectName("popup_muted_lbl");
            lay->addWidget(none_lbl);
            return;
        }

        for (const member_item_t &m : members) {
            QWidget *card = new QWidget(container_ptr);
            card->setObjectName("role_card");
            QHBoxLayout *c_lay = new QHBoxLayout(card);
            c_lay->setContentsMargins(8, 6, 8, 6);
            c_lay->setSpacing(8);

            QLabel *avt_lbl = new QLabel(card);
            avt_lbl->setFixedSize(32, 32);
            avt_lbl->setCursor(Qt::PointingHandCursor);

            QPixmap p = def_p;
            if (!m.avatar.isEmpty() && runtime_avatar_pixmaps.contains(m.avatar)) {
                QPixmap loaded = runtime_avatar_pixmaps.value(m.avatar);
                if (!loaded.isNull()) p = loaded;
            } else if (!m.avatar.isEmpty()) {
                user_request_file_download(0, m.avatar, 0);
            }
            avt_lbl->setPixmap(crop_avatar_cover(p, QSize(32, 32), 16));
            avt_lbl->setProperty("avatar_path", m.avatar);
            avt_lbl->setToolTip(QString("Click to view %1's avatar").arg(m.name));

            class ServerUsersAvtFilter : public QObject {
            public:
                QString avt_path;
                QString uname;
                explicit ServerUsersAvtFilter(const QString &a, const QString &n, QObject *parent = nullptr)
                    : QObject(parent), avt_path(a), uname(n) {}
            protected:
                bool eventFilter(QObject *obj, QEvent *event) override {
                    if (event->type() == QEvent::MouseButtonPress) {
                        QMouseEvent *me = static_cast<QMouseEvent*>(event);
                        if (me->button() == Qt::LeftButton) {
                            view_avatar_full(avt_path, uname);
                            return true;
                        }
                    }
                    return QObject::eventFilter(obj, event);
                }
            };
            avt_lbl->installEventFilter(new ServerUsersAvtFilter(m.avatar, m.name, avt_lbl));
            c_lay->addWidget(avt_lbl, 0);

            QVBoxLayout *name_col = new QVBoxLayout();
            name_col->setSpacing(2);
            name_col->setContentsMargins(0, 0, 0, 0);

            QLabel *name_lbl = new QLabel(m.name, card);
            name_lbl->setObjectName("role_name_lbl");
            name_lbl->setCursor(Qt::PointingHandCursor);
            name_col->addWidget(name_lbl);

            QString sub_text = (m.is_online ? "● Online" : "○ Offline");
            if (m.is_admin) sub_text += "  ⭐ Admin";
            QLabel *sub_lbl = new QLabel(sub_text, card);
            sub_lbl->setObjectName(m.is_online ? "member_status_online" : "member_status_offline");
            name_col->addWidget(sub_lbl);
            c_lay->addLayout(name_col, 1);

            uint64_t target_user_id = m.id;
            QString target_name = m.name;
            QString target_avt = m.avatar;

            if (target_user_id != client.user_id) {
                QPushButton *poke_btn = new QPushButton("Poke", card);
                poke_btn->setObjectName("popup_secondary_btn");
                poke_btn->setCursor(Qt::PointingHandCursor);
                poke_btn->setFixedHeight(24);
                QObject::connect(poke_btn, &QPushButton::clicked, [target_user_id]() {
                    user_poke_member(target_user_id);
                });
                c_lay->addWidget(poke_btn, 0);
            }

            auto open_profile = [parent, target_user_id, target_name, target_avt]() {
                open_user_info_popup(parent, target_user_id, target_name, target_avt);
            };

            auto open_user_roles = [parent, target_user_id, target_name]() {
                open_user_roles_popup(parent, target_user_id, target_name);
            };

            card->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(card, &QWidget::customContextMenuRequested, [card, target_user_id, target_name, open_profile, open_user_roles](const QPoint &pos) {
                QMenu menu(card);
                menu.setObjectName("chat_context_menu");
                QAction *prof_act = menu.addAction(QString("View Profile: %1").arg(target_name));
                QAction *roles_act = menu.addAction(QString("Roles: %1").arg(target_name));
                QObject::connect(roles_act, &QAction::triggered, open_user_roles);
                QObject::connect(prof_act, &QAction::triggered, open_profile);
                if (target_user_id != client.user_id) {
                    QAction *poke_act = menu.addAction(QString("Poke %1").arg(target_name));
                    QObject::connect(poke_act, &QAction::triggered, [target_user_id]() {
                        user_poke_member(target_user_id);
                    });
                }
                menu.exec(card->mapToGlobal(pos));
            });

            lay->addWidget(card);
        }
    };

    auto prev_avatar_ready = on_runtime_avatar_ready;
    on_runtime_avatar_ready = [container_ptr, prev_avatar_ready](const QString &avt_path) {
        if (prev_avatar_ready) prev_avatar_ready(avt_path);
        if (!container_ptr || avt_path.isEmpty()) return;
        if (!runtime_avatar_pixmaps.contains(avt_path)) return;
        QPixmap loaded = runtime_avatar_pixmaps.value(avt_path);
        if (loaded.isNull()) return;
        QPixmap cropped = crop_avatar_cover(loaded, QSize(32, 32), 16);
        for (QLabel *lbl : container_ptr->findChildren<QLabel*>()) {
            if (lbl->property("avatar_path").toString() == avt_path) {
                lbl->setPixmap(cropped);
            }
        }
    };

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);
    QObject::connect(popup, &QObject::destroyed, [prev_avatar_ready]() {
        on_member_list_received = nullptr;
        on_runtime_avatar_ready = prev_avatar_ready;
    });

    user_request_members();

    popup->raise();
    popup->show();
    return popup;
}

#endif // SERVER_USERS_POPUP_H
