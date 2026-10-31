#ifndef VOICE_PANEL_H
#define VOICE_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QIcon>
#include "functions/voice.h"
#include "utils/theme.h"

struct voice_panel_t {
    QWidget *widget = nullptr;
    QLabel *room_lbl = nullptr;
    QPushButton *mute_btn = nullptr;
    QPushButton *deaf_btn = nullptr;
    QPushButton *disconnect_btn = nullptr;
    bool server_connected = false;
};

inline void update_voice_panel_icons(voice_panel_t &vp) {
    QString icons_dir = QCoreApplication::applicationDirPath() + "/assets/icons/";
    if (vp.mute_btn) {
        QString icon = voice_ui_state.is_mute ? "mic_off.svg" : "mic_on.svg";
        QColor color = voice_ui_state.is_mute ? QColor(global_theme.destructive) : QColor(global_theme.foreground);
        vp.mute_btn->setIcon(QIcon(colored_svg(icons_dir + icon, color, QSize(18, 18))));
        vp.mute_btn->setText("");
        vp.mute_btn->setToolTip(voice_ui_state.is_mute ? "Unmute Microphone" : "Mute Microphone");
    }
    if (vp.deaf_btn) {
        QString icon = voice_ui_state.is_deaf ? "speaker_off.svg" : "speaker_on.svg";
        QColor color = voice_ui_state.is_deaf ? QColor(global_theme.destructive) : QColor(global_theme.foreground);
        vp.deaf_btn->setIcon(QIcon(colored_svg(icons_dir + icon, color, QSize(18, 18))));
        vp.deaf_btn->setText("");
        vp.deaf_btn->setToolTip(voice_ui_state.is_deaf ? "Enable Playback" : "Disable Playback");
    }
    if (vp.widget && vp.widget->parentWidget()) {
        QPushButton *sb_btn = vp.widget->parentWidget()->findChild<QPushButton*>("sidebar_settings_btn");
        if (sb_btn) {
            sb_btn->setIcon(QIcon(colored_svg(
                icons_dir + "settings.svg",
                QColor(global_theme.foreground), QSize(18, 18))));
        }
    }
}

inline voice_panel_t create_voice_panel(QWidget *parent) {
    voice_panel_t vp;
    vp.widget = new QWidget(parent);
    vp.widget->setObjectName("voice_panel");
    vp.widget->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(vp.widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    vp.room_lbl = new QLabel("Voice controls", vp.widget);
    vp.room_lbl->setObjectName("voice_room_lbl");
    layout->addWidget(vp.room_lbl);

    QHBoxLayout *btn_lay = new QHBoxLayout();
    btn_lay->setContentsMargins(0, 0, 0, 0);
    btn_lay->setSpacing(4);

    vp.mute_btn = new QPushButton(vp.widget);
    vp.mute_btn->setObjectName("voice_ctrl_btn");
    vp.mute_btn->setFixedSize(34, 30);
    vp.mute_btn->setToolTip("Mute Microphone");
    vp.mute_btn->setCursor(Qt::PointingHandCursor);

    vp.deaf_btn = new QPushButton(vp.widget);
    vp.deaf_btn->setObjectName("voice_ctrl_btn");
    vp.deaf_btn->setFixedSize(34, 30);
    vp.deaf_btn->setToolTip("Deafen Audio");
    vp.deaf_btn->setCursor(Qt::PointingHandCursor);

    vp.disconnect_btn = new QPushButton("Disconnect", vp.widget);
    vp.disconnect_btn->setObjectName("voice_dc_btn");
    vp.disconnect_btn->setCursor(Qt::PointingHandCursor);
    vp.disconnect_btn->hide();

    update_voice_panel_icons(vp);

    QObject::connect(vp.mute_btn, &QPushButton::clicked, []() {
        toggle_voice_mute();
    });
    QObject::connect(vp.deaf_btn, &QPushButton::clicked, []() {
        toggle_voice_deaf();
    });
    QObject::connect(vp.disconnect_btn, &QPushButton::clicked, []() {
        stop_voice_channel(true);
    });

    btn_lay->addWidget(vp.mute_btn);
    btn_lay->addWidget(vp.deaf_btn);
    btn_lay->addWidget(vp.disconnect_btn, 1);
    btn_lay->setAlignment(Qt::AlignLeft);
    layout->addLayout(btn_lay);

    vp.widget->hide();
    return vp;
}

inline void update_voice_panel_ui(voice_panel_t &vp, bool active, uint64_t room_id, const QString &room_name) {
    Q_UNUSED(room_id);
    if (!vp.widget) return;

    if (!vp.server_connected) {
        vp.widget->hide();
        return;
    }

    if (vp.room_lbl) {
        vp.room_lbl->setText(active ? "Voice: " + room_name : "Voice controls");
    }
    update_voice_panel_icons(vp);
    if (vp.disconnect_btn) vp.disconnect_btn->setVisible(active);
    vp.widget->show();
}

inline void set_voice_panel_server_connected(voice_panel_t &vp, bool connected) {
    vp.server_connected = connected;
    update_voice_panel_ui(
        vp, voice_ui_state.current_room_id > 0,
        voice_ui_state.current_room_id, voice_ui_state.current_room_name);
}

#endif // VOICE_PANEL_H
