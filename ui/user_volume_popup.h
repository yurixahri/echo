#ifndef USER_VOLUME_POPUP_H
#define USER_VOLUME_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include "functions/voice.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_user_volume_popup(QWidget *parent, uint64_t target_user_id, const QString &username, int gap_x = 24, int gap_y = 25) {
    if (!parent || target_user_id == 0) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("User Volume");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *desc_label = new QLabel(QString("Adjust volume for %1:").arg(username));
    desc_label->setObjectName("popup_muted_lbl");
    desc_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(desc_label);

    int cur_vol = voice_get_user_volume(target_user_id);
    QLabel *vol_label = new QLabel(QString("%1%").arg(cur_vol));
    vol_label->setObjectName("popup_section_title");
    vol_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(vol_label);

    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 200);
    slider->setValue(cur_vol);
    layout->addWidget(slider);

    QHBoxLayout *btn_lay = new QHBoxLayout();
    btn_lay->setSpacing(8);

    QPushButton *reset_btn = new QPushButton("Reset (100%)");
    reset_btn->setObjectName("popup_secondary_btn");
    reset_btn->setCursor(Qt::PointingHandCursor);
    reset_btn->setFixedHeight(28);

    QPushButton *close_btn = new QPushButton("Done");
    close_btn->setObjectName("popup_primary_btn");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setFixedHeight(28);

    btn_lay->addWidget(reset_btn);
    btn_lay->addWidget(close_btn);
    layout->addLayout(btn_lay);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->panel->setLayout(layout);

    QObject::connect(slider, &QSlider::valueChanged, [target_user_id, vol_label](int val) {
        vol_label->setText(QString("%1%").arg(val));
        voice_set_user_volume(target_user_id, val);
    });

    QObject::connect(reset_btn, &QPushButton::clicked, [slider]() {
        slider->setValue(100);
    });

    QObject::connect(close_btn, &QPushButton::clicked, popup, &QObject::deleteLater);

    popup->raise();
    popup->show();
    return popup;
}

#endif // USER_VOLUME_POPUP_H
