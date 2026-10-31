#include "qcollapsewidget.h"
#include "utils/theme.h"
#include <QEvent>

QCollapseWidget::QCollapseWidget(QString text, QString icon_name, QWidget *parent)
    : QWidget{parent}, current_icon_name{icon_name}
{
    QVBoxLayout *parent_layout = new QVBoxLayout();
    header = new QWidget();
    content = new QWidget();
    parent_layout->addWidget(header);
    parent_layout->addWidget(content);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    parent_layout->setSpacing(0);

    QVBoxLayout *content_layout = new QVBoxLayout();
    content_layout->setContentsMargins(6, 6, 6, 6);
    content_layout->setSpacing(6);
    content->setLayout(content_layout);

    QHBoxLayout *header_layout = new QHBoxLayout();
    title = new QLabel(text);
    title->setObjectName("title");
    if (!icon_name.isEmpty()){
        icon = new QLabel();
        icon->setPixmap(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/" + icon_name, QColor(global_theme.foreground), QSize(16, 16)) );
    }
    collapse = new QPushButton();
    collapse->setIcon( QIcon( colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/down_arrow.svg", QColor(global_theme.foreground), QSize(16, 16)) ) );
    header_layout->addWidget(icon);
    header_layout->addWidget(title);
    header_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    header_layout->addWidget(collapse);
    header_layout->setAlignment(Qt::AlignVCenter);
    header_layout->setContentsMargins(6, 6, 6, 6);
    header_layout->setSpacing(6);
    header->setLayout(header_layout);
    setLayout(parent_layout);
    setAttribute(Qt::WA_StyledBackground, true);

    header->installEventFilter(this);
    connect(collapse, &QPushButton::clicked, this, &QCollapseWidget::expand);
}

bool QCollapseWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == header && event->type() == QEvent::MouseButtonPress) {
        expand();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void QCollapseWidget::expand(){
    expanded = !expanded;
    content->setVisible(expanded);
    QString arrow = expanded ? "down_arrow.svg" : "left_arrow.svg";
    collapse->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/" + arrow, QColor(global_theme.foreground), QSize(16, 16))));
}

void QCollapseWidget::update_theme_icons() {
    if (!current_icon_name.isEmpty() && icon) {
        icon->setPixmap(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/" + current_icon_name, QColor(global_theme.foreground), QSize(16, 16)));
    }
    if (collapse) {
        QString arrow = expanded ? "down_arrow.svg" : "left_arrow.svg";
        collapse->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/" + arrow, QColor(global_theme.foreground), QSize(16, 16))));
    }
}
