#ifndef ROOM_VIEW_H
#define ROOM_VIEW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QCoreApplication>
#include <QMap>
#include <QStyle>
#include <QMenu>
#include "common/types.h"
#include "utils/theme.h"
#include "functions/file.h"
#include "functions/room.h"
#include "functions/voice.h"
#include "ui/password_popup.h"
#include "ui/user_roles_popup.h"
#include "ui/room_settings_popup.h"
#include "ui/user_info_popup.h"
#include "ui/user_volume_popup.h"

inline QMap<QString, QPixmap> room_icon_cache;

inline QWidget* get_room_popup_anchor(QWidget *widget) {
    QWidget *w = widget;
    while (w) {
        QVariant v = w->property("popup_anchor");
        if (v.isValid() && v.value<void*>()) {
            return static_cast<QWidget*>(v.value<void*>());
        }
        w = w->parentWidget();
    }
    return widget ? widget->window() : nullptr;
}

inline QPixmap room_avatar_for_display(const QString &avatar_path) {
    if (avatar_path.isEmpty()) {
        static QPixmap default_cropped;
        if (default_cropped.isNull()) {
            QPixmap def_src(QCoreApplication::applicationDirPath() + "/assets/defaults/user.png");
            default_cropped = crop_avatar_cover(def_src, QSize(32, 32), 16);
        }
        return default_cropped;
    }

    if (room_avatar_crop_cache.contains(avatar_path)) {
        return room_avatar_crop_cache.value(avatar_path);
    }

    if (runtime_avatar_pixmaps.contains(avatar_path)) {
        QPixmap source = runtime_avatar_pixmaps.value(avatar_path);
        if (!source.isNull()) {
            QPixmap cropped = crop_avatar_cover(source, QSize(32, 32), 16);
            if (!cropped.isNull()) {
                room_avatar_crop_cache.insert(avatar_path, cropped);
                return cropped;
            }
        }
    } else {
        if (!runtime_avatar_buffers.contains(avatar_path)) {
            user_request_file_download(0, avatar_path, 0);
        }
    }

    static QPixmap fallback_default;
    if (fallback_default.isNull()) {
        QPixmap def_src(QCoreApplication::applicationDirPath() + "/assets/defaults/user.png");
        fallback_default = crop_avatar_cover(def_src, QSize(32, 32), 16);
    }
    return fallback_default;
}

inline QPixmap room_colored_icon(const QString &icon_name, const QString &color, const QSize &size) {
    QString cache_key = icon_name + color + QString::number(size.width()) + "x" +
                        QString::number(size.height());
    if (room_icon_cache.contains(cache_key)) return room_icon_cache.value(cache_key);

    QPixmap icon = colored_svg(
        QCoreApplication::applicationDirPath() + "/assets/icons/" + icon_name,
        QColor(color), size);
    if (!icon.isNull()) room_icon_cache.insert(cache_key, icon);
    return icon;
}

inline void try_join_room_from_view(
    QWidget *room_container, uint64_t room_id, const QString &room_name,
    bool has_password, int join_type) {
    if (has_password) {
        if (client.server.room_passwords.contains(room_id) &&
            !client.server.room_passwords[room_id].isEmpty()) {
            user_join_room(room_id, client.server.room_passwords[room_id], join_type);
            if (join_type == 0 && on_room_chat_clicked) {
                on_room_chat_clicked(room_id, room_name);
            }
        } else {
            open_room_password_popup(get_room_popup_anchor(room_container), room_id, room_name, join_type);
        }
    } else {
        user_join_room(room_id, "", join_type);
        if (join_type == 0 && on_room_chat_clicked) {
            on_room_chat_clicked(room_id, room_name);
        }
    }
}

inline void update_room_speaking_indicators(QWidget *room_container) {
    if (!room_container) return;
    const QList<QLabel *> avatar_labels = room_container->findChildren<QLabel *>();
    for (QLabel *lbl : avatar_labels) {
        if (lbl->objectName() != "user_avatar_label" && lbl->objectName() != "user_avatar_label_speaking") continue;
        uint64_t uid = lbl->property("user_id").toULongLong();
        bool speaking = voice_ui_state.active_speakers.contains(uid);
        const char *new_name = speaking ? "user_avatar_label_speaking" : "user_avatar_label";
        if (lbl->objectName() != new_name) {
            lbl->setObjectName(new_name);
            lbl->style()->unpolish(lbl);
            lbl->style()->polish(lbl);
        }
    }
}

inline void update_room_view(QWidget *room_container, const QList<room_t> &rooms) {
    if (!room_container) return;

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(room_container->layout());
    if (!layout) {
        layout = new QVBoxLayout(room_container);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(8);
        layout->setAlignment(Qt::AlignTop);
    }

    // Check if we can update in-place without rebuilding widgets
    QList<QWidget*> existing_cards;
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (item && item->widget() && item->widget()->objectName() == "room_card") {
            existing_cards.append(item->widget());
        }
    }

    bool can_update_in_place = (!rooms.isEmpty() && existing_cards.size() == rooms.size());
    if (can_update_in_place) {
        for (int i = 0; i < rooms.size(); ++i) {
            QWidget *card = existing_cards[i];
            if (card->property("room_id").toULongLong() != rooms[i].id ||
                card->property("room_name").toString() != rooms[i].name ||
                card->property("has_password").toBool() != rooms[i].has_password ||
                card->property("needed_voice_join_power").toUInt() != rooms[i].needed_voice_join_power ||
                card->property("needed_chat_join_power").toUInt() != rooms[i].needed_chat_join_power ||
                card->property("needed_speak_power").toUInt() != rooms[i].needed_speak_power) {
                can_update_in_place = false;
                break;
            }
            QList<QWidget*> u_rows = card->findChildren<QWidget*>("user_row");
            if (u_rows.size() != rooms[i].users.size()) {
                can_update_in_place = false;
                break;
            }
            for (int u = 0; u < rooms[i].users.size(); ++u) {
                if (u_rows[u]->property("user_id").toULongLong() != rooms[i].users[u].id) {
                    can_update_in_place = false;
                    break;
                }
            }
            if (!can_update_in_place) break;
        }
    }

    if (can_update_in_place) {
        for (int i = 0; i < rooms.size(); ++i) {
            QWidget *card = existing_cards[i];
            const room_t &room = rooms[i];

            QPushButton *call_btn = card->findChild<QPushButton*>("room_call_btn");
            if (call_btn) {
                bool is_voice_active = (voice_ui_state.current_room_id == room.id);
                QString call_icon_color = is_voice_active ? global_theme.destructive : global_theme.foreground;
                call_btn->setIcon(QIcon(room_colored_icon("phone.svg", call_icon_color, QSize(16, 16))));
            }

            QPushButton *chat_btn = card->findChild<QPushButton*>("room_action_btn");
            if (chat_btn) {
                chat_btn->setIcon(QIcon(room_colored_icon("dialog.svg", global_theme.foreground, QSize(16, 16))));
            }

            QLabel *count_lbl = card->findChild<QLabel*>("room_card_users");
            if (count_lbl) {
                int user_count = room.users.size();
                count_lbl->setText(QString("%1 user%2").arg(user_count).arg(user_count == 1 ? "" : "s"));
            }

            QList<QWidget*> u_rows = card->findChildren<QWidget*>("user_row");
            for (int u = 0; u < room.users.size(); ++u) {
                QWidget *user_row = u_rows[u];
                const voice_user_t &user = room.users[u];

                QLabel *avt_lbl = user_row->findChild<QLabel*>("user_avatar_label");
                if (!avt_lbl) avt_lbl = user_row->findChild<QLabel*>("user_avatar_label_speaking");
                if (avt_lbl) {
                    avt_lbl->setPixmap(room_avatar_for_display(user.avatar));
                }

                QLabel *user_name_lbl = user_row->findChild<QLabel*>("user_card_name");
                if (user_name_lbl && user_name_lbl->text() != user.name) {
                    user_name_lbl->setText(user.name);
                }

                QLabel *mic_lbl = user_row->findChild<QLabel*>("user_mic_icon");
                if (mic_lbl) {
                    mic_lbl->setPixmap(room_colored_icon("mic_off.svg", global_theme.destructive, QSize(16, 16)));
                    mic_lbl->setVisible(user.is_mute);
                }

                QLabel *deaf_lbl = user_row->findChild<QLabel*>("user_deaf_icon");
                if (deaf_lbl) {
                    deaf_lbl->setPixmap(room_colored_icon("speaker_off.svg", global_theme.destructive, QSize(16, 16)));
                    deaf_lbl->setVisible(user.is_deaf);
                }
            }
        }
        update_room_speaking_indicators(room_container);
        return;
    }

    // Clear previous widgets when structure changes
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (rooms.isEmpty()) {
        QLabel *empty_lbl = new QLabel("No rooms available. Create a room from the server menu.", room_container);
        empty_lbl->setObjectName("empty_rooms_label");
        empty_lbl->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty_lbl);
        return;
    }

    for (const room_t &room : rooms) {
        QWidget *room_card = new QWidget(room_container);
        room_card->setObjectName("room_card");
        room_card->setProperty("room_id", (qulonglong)room.id);
        room_card->setProperty("room_name", room.name);
        room_card->setProperty("has_password", room.has_password);
        room_card->setProperty("needed_voice_join_power", room.needed_voice_join_power);
        room_card->setProperty("needed_chat_join_power", room.needed_chat_join_power);
        room_card->setProperty("needed_speak_power", room.needed_speak_power);
        QVBoxLayout *card_layout = new QVBoxLayout(room_card);
        card_layout->setContentsMargins(0, 0, 0, 0);
        card_layout->setSpacing(4);

        // Header row
        QWidget *header_row = new QWidget(room_card);
        header_row->setObjectName("room_header_row");
        header_row->setAttribute(Qt::WA_StyledBackground, true);
        QHBoxLayout *row_layout = new QHBoxLayout(header_row);
        row_layout->setContentsMargins(10, 8, 10, 8);
        row_layout->setSpacing(6);

        QLabel *name_lbl = new QLabel(room.name, header_row);
        name_lbl->setObjectName("room_card_name");
        name_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        row_layout->addWidget(name_lbl, 0);

        QLabel *lock_lbl = new QLabel(header_row);
        lock_lbl->setObjectName("room_card_lock");
        lock_lbl->setFixedSize(14, 14);
        lock_lbl->setPixmap(room_colored_icon("lock.svg", global_theme.muted_foreground, QSize(14, 14)));
        lock_lbl->setVisible(room.has_password);
        lock_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        row_layout->addWidget(lock_lbl, 0);

        row_layout->addStretch(1);

        int user_count = room.users.size();
        QString count_text = QString("%1 user%2").arg(user_count).arg(user_count == 1 ? "" : "s");
        QLabel *count_lbl = new QLabel(count_text, header_row);
        count_lbl->setObjectName("room_card_users");
        count_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        row_layout->addWidget(count_lbl, 0);

        header_row->setContextMenuPolicy(Qt::CustomContextMenu);
        room_t current_room_data = room;
        QObject::connect(header_row, &QWidget::customContextMenuRequested, [header_row, current_room_data, room_container](const QPoint &pos) {
            if (!client.is_admin) return;
            QMenu menu(header_row);
            menu.setObjectName("room_context_menu");
            QAction *settings_act = menu.addAction(QString("Room Settings: %1").arg(current_room_data.name));
            QObject::connect(settings_act, &QAction::triggered, [header_row, current_room_data, room_container]() {
                open_room_settings_popup(get_room_popup_anchor(room_container ? room_container : header_row), current_room_data);
            });
            menu.exec(header_row->mapToGlobal(pos));
        });

        // Voice call button
        QPushButton *call_btn = new QPushButton(header_row);
        call_btn->setObjectName("room_call_btn");
        call_btn->setCursor(Qt::PointingHandCursor);
        call_btn->setFixedSize(26, 26);
        call_btn->setToolTip("Join Voice");
        bool is_voice_active = (voice_ui_state.current_room_id == room.id);
        QString call_icon_color = is_voice_active ? global_theme.destructive : global_theme.foreground;
        call_btn->setIcon(QIcon(room_colored_icon("phone.svg", call_icon_color, QSize(16, 16))));
        uint64_t rid = room.id;
        QString rname = room.name;
        bool has_pwd = room.has_password;
        QObject::connect(call_btn, &QPushButton::clicked, [room_container, rid, rname, has_pwd]() {
            if (voice_ui_state.current_room_id == rid) {
                stop_voice_channel(true);
            } else {
                try_join_room_from_view(room_container, rid, rname, has_pwd, 1);
            }
        });
        row_layout->addWidget(call_btn, 0);

        // Chat button
        QPushButton *chat_btn = new QPushButton(header_row);
        chat_btn->setObjectName("room_action_btn");
        chat_btn->setCursor(Qt::PointingHandCursor);
        chat_btn->setFixedSize(26, 26);
        chat_btn->setToolTip("Open Chat");
        chat_btn->setIcon(QIcon(room_colored_icon("dialog.svg", global_theme.foreground, QSize(16, 16))));
        QObject::connect(chat_btn, &QPushButton::clicked, [room_container, rid, rname, has_pwd]() {
            try_join_room_from_view(room_container, rid, rname, has_pwd, 0);
        });
        row_layout->addWidget(chat_btn, 0);

        card_layout->addWidget(header_row);

        // Connected voice users list
        for (const voice_user_t &user : room.users) {
            QWidget *user_row = new QWidget(room_card);
            user_row->setObjectName("user_row");
            user_row->setProperty("user_id", (qulonglong)user.id);
            user_row->setContextMenuPolicy(Qt::CustomContextMenu);
            user_row->setCursor(Qt::PointingHandCursor);
            uint64_t target_user_id = user.id;
            QString target_uname = user.name;
            uint64_t in_room_id = room.id;
            QString target_avatar = user.avatar;
            QObject::connect(user_row, &QWidget::customContextMenuRequested, [user_row, target_user_id, target_uname, target_avatar, in_room_id, room_container](const QPoint &pos) {
                bool can_manage_roles = client.is_admin;
                bool can_kick = client.is_admin || client.can_kick;

                QMenu menu(user_row);
                menu.setObjectName("room_user_context_menu");

                QAction *profile_act = menu.addAction(QString("View Profile: %1").arg(target_uname));
                QObject::connect(profile_act, &QAction::triggered, [user_row, target_user_id, target_uname, target_avatar, room_container]() {
                    open_user_info_popup(get_room_popup_anchor(room_container ? room_container : user_row), target_user_id, target_uname, target_avatar);
                });

                if (target_user_id != client.user_id) {
                    QAction *poke_act = menu.addAction(QString("Poke %1").arg(target_uname));
                    QObject::connect(poke_act, &QAction::triggered, [target_user_id]() {
                        user_poke_member(target_user_id);
                    });

                    int vol = voice_get_user_volume(target_user_id);
                    QAction *vol_act = menu.addAction(QString("Volume (%1%)...").arg(vol));
                    QObject::connect(vol_act, &QAction::triggered, [user_row, target_user_id, target_uname, room_container]() {
                        open_user_volume_popup(get_room_popup_anchor(room_container ? room_container : user_row), target_user_id, target_uname);
                    });
                }

                if (can_manage_roles) {
                    QAction *roles_act = menu.addAction(QString("Roles: %1").arg(target_uname));
                    QObject::connect(roles_act, &QAction::triggered, [user_row, target_user_id, target_uname, room_container]() {
                        open_user_roles_popup(get_room_popup_anchor(room_container ? room_container : user_row), target_user_id, target_uname);
                    });
                }
                if (can_kick && target_user_id != client.user_id) {
                    QAction *kick_act = menu.addAction(QString("Kick: %1").arg(target_uname));
                    QObject::connect(kick_act, &QAction::triggered, [target_user_id, in_room_id]() {
                        user_kick_member(target_user_id, in_room_id);
                    });
                }
                menu.exec(user_row->mapToGlobal(pos));
            });

            QHBoxLayout *u_lay = new QHBoxLayout(user_row);
            u_lay->setContentsMargins(18, 2, 10, 2);
            u_lay->setSpacing(8);

            QLabel *avt_lbl = new QLabel(user_row);
            bool speaking = voice_ui_state.active_speakers.contains(user.id);
            avt_lbl->setObjectName(speaking ? "user_avatar_label_speaking" : "user_avatar_label");
            avt_lbl->setFixedSize(36, 36);
            avt_lbl->setAlignment(Qt::AlignCenter);
            avt_lbl->setProperty("user_id", QVariant::fromValue<qulonglong>(user.id));
            avt_lbl->setPixmap(room_avatar_for_display(user.avatar));
            avt_lbl->setCursor(Qt::PointingHandCursor);
            avt_lbl->setToolTip(QString("Click to view %1's avatar").arg(target_uname));

            class RoomAvtClickFilter : public QObject {
            public:
                QString avt_path;
                QString uname;
                explicit RoomAvtClickFilter(const QString &a, const QString &n, QObject *parent = nullptr)
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
            avt_lbl->installEventFilter(new RoomAvtClickFilter(target_avatar, target_uname, avt_lbl));
            u_lay->addWidget(avt_lbl, 0);

            QLabel *user_name_lbl = new QLabel(user.name, user_row);
            user_name_lbl->setObjectName("user_card_name");
            user_name_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            u_lay->addWidget(user_name_lbl, 1);

            QLabel *mic_off_lbl = new QLabel(user_row);
            mic_off_lbl->setObjectName("user_mic_icon");
            mic_off_lbl->setPixmap(room_colored_icon("mic_off.svg", global_theme.destructive, QSize(16, 16)));
            mic_off_lbl->setFixedSize(16, 16);
            mic_off_lbl->setVisible(user.is_mute);
            mic_off_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            u_lay->addWidget(mic_off_lbl, 0);

            QLabel *deaf_lbl = new QLabel(user_row);
            deaf_lbl->setObjectName("user_deaf_icon");
            deaf_lbl->setPixmap(room_colored_icon("speaker_off.svg", global_theme.destructive, QSize(16, 16)));
            deaf_lbl->setFixedSize(16, 16);
            deaf_lbl->setVisible(user.is_deaf);
            deaf_lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            u_lay->addWidget(deaf_lbl, 0);

            card_layout->addWidget(user_row);
        }

        layout->addWidget(room_card);
    }

    layout->addStretch(1);
}

inline QScrollArea* create_room_scroll_view(QWidget *parent, QWidget **out_container, QWidget *popup_anchor = nullptr) {
    QScrollArea *scroll_area = new QScrollArea(parent);
    scroll_area->setObjectName("room_scroll_area");
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *container = new QWidget();
    container->setObjectName("room_container");

    if (popup_anchor) {
        container->setProperty("popup_anchor", QVariant::fromValue<void*>((void*)popup_anchor));
    }

    QVBoxLayout *container_layout = new QVBoxLayout(container);
    container_layout->setContentsMargins(6, 6, 6, 6);
    container_layout->setSpacing(8);
    container_layout->setAlignment(Qt::AlignTop);

    scroll_area->setWidget(container);
    if (out_container) *out_container = container;
    return scroll_area;
}

#endif // ROOM_VIEW_H
