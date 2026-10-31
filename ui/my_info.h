#ifndef MY_INFO_H
#define MY_INFO_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDateTime>
#include <QMovie>
#include <QBuffer>
#include "functions/user.h"
#include "functions/file.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QString format_uptime(uint64_t seconds) {
    uint64_t d = seconds / 86400;
    uint64_t h = (seconds % 86400) / 3600;
    uint64_t m = (seconds % 3600) / 60;
    uint64_t s = seconds % 60;
    if (d > 0) return QString("%1d %2h %3m").arg(d).arg(h).arg(m);
    if (h > 0) return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    return QString("%1m %2s").arg(m).arg(s);
}

inline QPopup* open_my_info_popup(QWidget *parent, int gap_x = 22, int gap_y = 20) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("My Profile");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    // Avatar display
    QLabel *avt_label = new QLabel();
    avt_label->setObjectName("my_info_avatar");
    avt_label->setAlignment(Qt::AlignCenter);
    avt_label->setFixedSize(72, 72);
    avt_label->setCursor(Qt::PointingHandCursor);
    avt_label->setToolTip("Click to view full image");

    class AvatarClickFilter : public QObject {
    public:
        std::function<void()> on_clicked;
        explicit AvatarClickFilter(std::function<void()> cb, QObject *parent = nullptr)
            : QObject(parent), on_clicked(cb) {}
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && on_clicked) {
                    on_clicked();
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };

    avt_label->installEventFilter(new AvatarClickFilter([]() {
        view_avatar_full(client.user_avatar, config.username);
    }, avt_label));

    QString default_avt_path = QCoreApplication::applicationDirPath() + "/assets/defaults/user.png";
    QPixmap def_pix(default_avt_path);
    if (!def_pix.isNull()) {
        avt_label->setPixmap(crop_avatar_cover(def_pix, QSize(68, 68), 34));
    }

    QPushButton *change_avt_btn = new QPushButton("Change Avatar");
    change_avt_btn->setObjectName("popup_secondary_btn");
    change_avt_btn->setCursor(Qt::PointingHandCursor);
    change_avt_btn->setFixedHeight(26);

    QVBoxLayout *avt_col = new QVBoxLayout();
    avt_col->setAlignment(Qt::AlignCenter);
    avt_col->setSpacing(6);
    avt_col->addWidget(avt_label, 0, Qt::AlignCenter);
    avt_col->addWidget(change_avt_btn, 0, Qt::AlignCenter);
    layout->addLayout(avt_col);

    // Name row
    QLabel *name_lbl = new QLabel("Username: " + config.username);
    name_lbl->setObjectName("popup_section_title");
    layout->addWidget(name_lbl);

    // UUID
    QLabel *uuid_title = new QLabel("Unique ID (UUID):");
    uuid_title->setObjectName("popup_muted_lbl");
    layout->addWidget(uuid_title);

    QLineEdit *uuid_edit = new QLineEdit(client.server.uuid);
    uuid_edit->setReadOnly(true);
    uuid_edit->setFixedHeight(28);
    layout->addWidget(uuid_edit);

    // Member since & Server Uptime
    QLabel *created_lbl = new QLabel("Member Since: Loading...");
    created_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(created_lbl);

    QLabel *uptime_lbl = new QLabel("Server Uptime: Loading...");
    uptime_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(uptime_lbl);

    QPushButton *close_btn = new QPushButton("Close");
    close_btn->setObjectName("popup_close_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(28);
    layout->addWidget(close_btn);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    auto apply_avatar = [avt_label, default_avt_path](const QString &avt_path, const QByteArray &data = QByteArray()) {
        if (!avt_label) return;
        if (avt_label->movie()) {
            QMovie *old_m = avt_label->movie();
            avt_label->setMovie(nullptr);
            delete old_m;
        }

        if (avt_path.isEmpty() && data.isEmpty()) {
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

        QByteArray buf_data = data;
        if (buf_data.isEmpty() && runtime_avatar_buffers.contains(avt_path)) {
            buf_data = runtime_avatar_buffers[avt_path];
        }

        if (buf_data.isEmpty()) {
            user_request_file_download(0, avt_path, 0);
            return;
        }

        QPixmap p;
        if (p.loadFromData(buf_data)) {
            if (p.width() > 128 || p.height() > 128) {
                p = p.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            avt_label->setPixmap(crop_avatar_cover(p, QSize(68, 68), 34));
        }
    };

    // Change avatar action
    QWidget *top_window = parent->window();
    QObject::connect(change_avt_btn, &QPushButton::clicked, [top_window, apply_avatar]() {
        QString file = QFileDialog::getOpenFileName(top_window, "Select Avatar (Max 2MB)", "",
            "Images (*.png *.jpg *.jpeg *.webp *.avif *.bmp *.gif)");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            if (fi.size() > 2 * 1024 * 1024) {
                if (on_operation_error) on_operation_error("Avatar image exceeds 2MB limit");
                return;
            }

            QFile f(file);
            if (!f.open(QIODevice::ReadOnly)) {
                if (on_operation_error) on_operation_error("Cannot open selected file");
                return;
            }
            QByteArray d = f.readAll();
            f.close();

            if (!is_valid_avatar_mime(d, file)) {
                if (on_operation_error) on_operation_error("Invalid or unsupported image format. Allowed: PNG, JPG, WEBP, AVIF, BMP, GIF");
                return;
            }

            apply_avatar(file, d);
            user_upload_avatar(file);
        }
    });

    on_my_info_received = [name_lbl, uuid_edit, created_lbl, uptime_lbl, apply_avatar](const user_info_t &info) {
        if (name_lbl) name_lbl->setText("Username: " + info.name);
        if (uuid_edit) uuid_edit->setText(info.uuid);
        if (created_lbl) {
            QString dt = QDateTime::fromSecsSinceEpoch(info.created_at).toString("yyyy-MM-dd HH:mm");
            created_lbl->setText("Member Since: " + dt);
        }
        if (uptime_lbl) {
            uptime_lbl->setText("Server Uptime: " + format_uptime(info.server_uptime));
        }

        if (info.avatar.isEmpty()) {
            apply_avatar("");
        } else {
            client.user_avatar = info.avatar;
            apply_avatar(info.avatar);
        }
    };

    on_avatar_updated = [apply_avatar](const QString &new_avt) {
        // qDebug() << "Avatar upload successful:" << new_avt;
        QString old_avt = client.user_avatar;
        if (!old_avt.isEmpty() && old_avt != new_avt) {
            runtime_avatar_pixmaps.remove(old_avt);
            runtime_avatar_buffers.remove(old_avt);
            room_avatar_crop_cache.remove(old_avt);
        }
        client.user_avatar = new_avt;
        user_request_file_download(0, new_avt, 0);
    };

    auto prev_avatar_ready = on_runtime_avatar_ready;
    on_runtime_avatar_ready = [apply_avatar, prev_avatar_ready](const QString &avt_path) {
        if (prev_avatar_ready) prev_avatar_ready(avt_path);
        apply_avatar(avt_path);
    };

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    QObject::connect(popup, &QObject::destroyed, [=]() {
        on_my_info_received = nullptr;
        on_avatar_updated = nullptr;
        on_runtime_avatar_ready = prev_avatar_ready;
    });

    user_request_my_info();

    popup->raise();
    popup->show();
    return popup;
}

#endif // MY_INFO_H
