#ifndef USER_ROLES_POPUP_H
#define USER_ROLES_POPUP_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QPointer>
#include "functions/role.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QPopup* open_user_roles_popup(QWidget *parent, uint64_t target_user_id, const QString &target_user_name, int gap_x = 22, int gap_y = 20) {
    if (!parent || target_user_id == 0) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel(QString("Roles for: %1").arg(target_user_name));
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QLabel *desc_label = new QLabel("Check roles to assign to this member:");
    desc_label->setObjectName("popup_desc");
    layout->addWidget(desc_label);

    QScrollArea *scroll = new QScrollArea();
    scroll->setObjectName("popup_scroll_area");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMaximumHeight(500);
    scroll->setMinimumHeight(300);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *roles_container = new QWidget();
    roles_container->setObjectName("popup_scroll_content");
    roles_container->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *roles_layout = new QVBoxLayout(roles_container);
    roles_layout->setContentsMargins(8, 8, 8, 8);
    roles_layout->setSpacing(6);
    roles_layout->setAlignment(Qt::AlignTop);

    QLabel *loading_lbl = new QLabel("Loading roles...");
    loading_lbl->setObjectName("popup_desc");
    roles_layout->addWidget(loading_lbl);

    scroll->setWidget(roles_container);
    layout->addWidget(scroll);

    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setObjectName("popup_close_btn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedHeight(30);

    QPushButton *save_btn = new QPushButton("Save");
    save_btn->setObjectName("popup_primary_btn");
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedHeight(30);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(save_btn);
    layout->addLayout(btn_layout);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->set_panel_width(360, 600);
    popup->panel->setLayout(layout);
    popup->setStyleSheet(get_roles_popup_qss(theme_path));

    QPointer<QWidget> container_ptr(roles_container);
    on_user_roles_received = [container_ptr, target_user_id](uint64_t uid, const QList<user_role_item_t> &roles) {
        if (!container_ptr || uid != target_user_id) return;
        QVBoxLayout *lay = qobject_cast<QVBoxLayout*>(container_ptr->layout());
        if (!lay) return;

        while (QLayoutItem *it = lay->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }

        if (roles.isEmpty()) {
            QLabel *none_lbl = new QLabel("No roles configured on this server.", container_ptr);
            none_lbl->setObjectName("popup_desc");
            lay->addWidget(none_lbl);
            return;
        }

        for (const user_role_item_t &r : roles) {
            QCheckBox *cb = new QCheckBox(r.name, container_ptr);
            cb->setChecked(r.assigned);
            cb->setProperty("role_id", (qulonglong)r.id);
            lay->addWidget(cb);
        }
    };

    auto submit_assign = [container_ptr, target_user_id, popup]() {
        if (!container_ptr) return;
        QStringList selected_ids;
        for (QCheckBox *cb : container_ptr->findChildren<QCheckBox*>()) {
            if (cb->isChecked()) {
                selected_ids.append(QString::number(cb->property("role_id").toULongLong()));
            }
        }
        user_assign_roles(target_user_id, selected_ids.join(','));
        popup->deleteLater();
    };

    QObject::connect(cancel_btn, &QPushButton::clicked, popup, &QObject::deleteLater);
    QObject::connect(save_btn, &QPushButton::clicked, popup, submit_assign);

    popup->raise();
    popup->show();

    user_request_user_roles(target_user_id);
    return popup;
}

#endif // USER_ROLES_POPUP_H
