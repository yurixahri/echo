#ifndef USER_INFO_POPUP_H
#define USER_INFO_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QPointer>
#include "functions/user.h"
#include "functions/file.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_user_info_popup(QWidget *parent, uint64_t target_user_id, const QString &initial_name, const QString &initial_avt = "", int gap_x = 24, int gap_y = 22) {
    if (!parent || target_user_id == 0) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("User Profile");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *avt_label = new QLabel();
    avt_label->setObjectName("my_info_avatar");
    avt_label->setAlignment(Qt::AlignCenter);
    avt_label->setFixedSize(72, 72);
    avt_label->setCursor(Qt::PointingHandCursor);
    avt_label->setToolTip(QString("Click to view %1's avatar").arg(initial_name));

    class UserInfoAvtClickFilter : public QObject {
    public:
        QString avt;
        QString name;
        explicit UserInfoAvtClickFilter(const QString &a, const QString &n, QObject *parent = nullptr)
            : QObject(parent), avt(a), name(n) {}
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    view_avatar_full(avt, name);
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };
    UserInfoAvtClickFilter *avt_filter = new UserInfoAvtClickFilter(initial_avt, initial_name, avt_label);
    avt_label->installEventFilter(avt_filter);

    QString default_avt_path = QCoreApplication::applicationDirPath() + "/assets/defaults/user.png";
    QPixmap def_pix(default_avt_path);
    if (!def_pix.isNull()) {
        avt_label->setPixmap(crop_avatar_cover(def_pix, QSize(68, 68), 34));
    }

    auto apply_avatar = [avt_label, default_avt_path](const QString &avt_path) {
        if (!avt_label) return;
        if (avt_path.isEmpty()) {
            QPixmap p(default_avt_path);
            if (!p.isNull()) avt_label->setPixmap(crop_avatar_cover(p, QSize(68, 68), 34));
            return;
        }

        if (runtime_avatar_pixmaps.contains(avt_path)) {
            QPixmap p = runtime_avatar_pixmaps.value(avt_path);
            if (!p.isNull()) {
                avt_label->setPixmap(crop_avatar_cover(p, QSize(68, 68), 34));
                return;
            }
        }

        if (runtime_avatar_buffers.contains(avt_path)) {
            QPixmap p;
            if (p.loadFromData(runtime_avatar_buffers[avt_path])) {
                avt_label->setPixmap(crop_avatar_cover(p, QSize(68, 68), 34));
                return;
            }
        }

        user_request_file_download(0, avt_path, 0);
    };

    if (!initial_avt.isEmpty()) {
        apply_avatar(initial_avt);
    }

    QVBoxLayout *avt_col = new QVBoxLayout();
    avt_col->setAlignment(Qt::AlignCenter);
    avt_col->addWidget(avt_label, 0, Qt::AlignCenter);
    layout->addLayout(avt_col);

    QLabel *name_lbl = new QLabel(initial_name);
    name_lbl->setObjectName("popup_section_title");
    name_lbl->setAlignment(Qt::AlignCenter);
    layout->addWidget(name_lbl);

    QLabel *admin_badge = new QLabel();
    admin_badge->setObjectName("popup_muted_lbl");
    admin_badge->setAlignment(Qt::AlignCenter);
    admin_badge->hide();
    layout->addWidget(admin_badge);

    QWidget *roles_container = new QWidget();
    QVBoxLayout *roles_vbox = new QVBoxLayout(roles_container);
    roles_vbox->setContentsMargins(0, 0, 0, 0);
    roles_vbox->setSpacing(4);
    roles_vbox->setAlignment(Qt::AlignCenter);

    QLabel *initial_role_lbl = new QLabel("User", roles_container);
    initial_role_lbl->setObjectName("popup_muted_lbl");
    initial_role_lbl->setAlignment(Qt::AlignCenter);
    roles_vbox->addWidget(initial_role_lbl);
    layout->addWidget(roles_container);

    QLabel *created_lbl = new QLabel("Member Since: Loading...");
    created_lbl->setObjectName("popup_muted_lbl");
    created_lbl->setAlignment(Qt::AlignCenter);
    layout->addWidget(created_lbl);

    QHBoxLayout *btn_lay = new QHBoxLayout();
    btn_lay->setSpacing(8);

    QPushButton *poke_btn = new QPushButton(QString("Poke %1").arg(initial_name));
    poke_btn->setObjectName("popup_primary_btn");
    poke_btn->setCursor(Qt::PointingHandCursor);
    poke_btn->setFixedHeight(28);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(28);

    btn_lay->addWidget(poke_btn);
    btn_lay->addWidget(close_btn);
    layout->addLayout(btn_lay);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->set_panel_width(320, 420);
    popup->panel->setLayout(layout);

    QObject::connect(poke_btn, &QPushButton::clicked, [target_user_id]() {
        user_poke_member(target_user_id);
    });

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    auto prev_avatar_ready = on_runtime_avatar_ready;
    on_runtime_avatar_ready = [apply_avatar, prev_avatar_ready](const QString &avt_path) {
        if (prev_avatar_ready) prev_avatar_ready(avt_path);
        apply_avatar(avt_path);
    };

    on_member_detail_received = [name_lbl, admin_badge, roles_container, roles_vbox, created_lbl, apply_avatar, target_user_id, avt_filter](const user_info_t &info) {
        if (info.id != target_user_id) return;
        if (name_lbl) name_lbl->setText(info.name);
        if (avt_filter) avt_filter->name = info.name;

        if (admin_badge) {
            if (info.is_admin) {
                admin_badge->setText("⭐ Server Administrator");
                admin_badge->setObjectName("user_profile_admin_badge");
                admin_badge->show();
            } else {
                admin_badge->hide();
            }
        }

        if (roles_container && roles_vbox) {
            while (QLayoutItem *child = roles_vbox->takeAt(0)) {
                if (child->widget()) child->widget()->deleteLater();
                delete child;
            }

            if (info.roles.isEmpty()) {
                if (!info.is_admin) {
                    QLabel *user_lbl = new QLabel("User", roles_container);
                    user_lbl->setObjectName("popup_muted_lbl");
                    user_lbl->setAlignment(Qt::AlignCenter);
                    roles_vbox->addWidget(user_lbl);
                }
            } else {
                int col = 0;
                QHBoxLayout *cur_row = nullptr;
                for (const QString &r : info.roles) {
                    if (col % 3 == 0) {
                        QWidget *row_w = new QWidget(roles_container);
                        cur_row = new QHBoxLayout(row_w);
                        cur_row->setContentsMargins(0, 0, 0, 0);
                        cur_row->setSpacing(5);
                        cur_row->setAlignment(Qt::AlignCenter);
                        roles_vbox->addWidget(row_w);
                    }
                    QLabel *pill = new QLabel(r, roles_container);
                    pill->setObjectName("user_profile_role_pill");
                    cur_row->addWidget(pill);
                    col++;
                }
            }
        }

        if (created_lbl) {
            if (info.created_at > 0) {
                QString dt = QDateTime::fromSecsSinceEpoch(info.created_at).toString("yyyy-MM-dd HH:mm");
                created_lbl->setText("Member Since: " + dt);
            } else {
                created_lbl->setText("");
            }
        }
        if (!info.avatar.isEmpty()) {
            if (avt_filter) avt_filter->avt = info.avatar;
            apply_avatar(info.avatar);
        }
    };

    QObject::connect(popup, &QObject::destroyed, [prev_avatar_ready]() {
        on_member_detail_received = nullptr;
        on_runtime_avatar_ready = prev_avatar_ready;
    });

    user_request_member_detail(target_user_id);

    popup->raise();
    popup->show();
    return popup;
}

#endif // USER_INFO_POPUP_H
