#include "qcollapsewidget.h"
#include "utils/theme.h"


QCollapseWidget::QCollapseWidget(QString text, QString icon_name, QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *parent_layout = new QVBoxLayout();
    header = new QWidget();
    content = new QWidget();
    parent_layout->addWidget(header);
    parent_layout->addWidget(content);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    parent_layout->setSpacing(0);

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
    header_layout->setContentsMargins(5, 2, 5, 2);
    header_layout->setSpacing(5);
    header->setLayout(header_layout);
    setLayout(parent_layout);
    setAttribute(Qt::WA_StyledBackground, true);
}

void QCollapseWidget::expand(){

}
