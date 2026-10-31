#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QSettings>
#include <QSvgRenderer>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <functional>

struct theme {
    QString background                 = "#1E1A21";
    QString foreground                 = "#E6DEE8";
    QString card                       = "#1A161D";
    QString card_foreground            = "#E6DEE8";
    QString popover                    = "#423a46";
    QString popover_foreground         = "#E6DEE8";
    QString primary                    = "#C69FE2";
    QString primary_foreground         = "#1E1A21";
    QString secondary                  = "#39323e";
    QString secondary_foreground       = "#E6DEE8";
    QString muted                      = "#2A202C";
    QString muted_foreground           = "#80748e";
    QString accent                     = "#1E1A21";
    QString accent_foreground          = "#E6DEE8";
    QString destructive                = "#F28BB2";
    QString destructive_foreground     = "#1E1A21";
    QString border                     = "#4c4650";
    QString input                      = "#3E3445";
    QString ring                       = "#947993";
    QString sidebar                    = "#1a161db6";
    QString sidebar_foreground         = "#E6DEE8";
    QString sidebar_primary            = "#C69FE2";
    QString sidebar_primary_foreground = "#1E1A21";
    QString sidebar_accent             = "#2A202C";
    QString sidebar_accent_foreground  = "#E6DEE8";
    QString sidebar_border             = "#3E3445";
    QString sidebar_ring               = "#6C5E7C";
    QString close                      = "#d13535";
};

extern struct theme global_theme;
extern QString theme_path;
inline std::function<void(const QString &new_theme)> on_theme_changed = nullptr;

inline QStringList get_available_themes() {
    QString styles_dir = QCoreApplication::applicationDirPath() + "/styles";
    QDir dir(styles_dir);
    QStringList themes = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!themes.contains("default")) {
        themes.prepend("default");
    }
    return themes;
}

inline QString get_combobox_qss() {
    return QString(
        "QComboBox {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 13px;"
        "}"
        "QComboBox:hover { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: %5;"
        "  color: %2;"
        "  selection-background-color: %4;"
        "  selection-color: %6;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 3px;"
        "  outline: 0px;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  background-color: transparent;"
        "  color: %2;"
        "  min-height: 24px;"
        "  padding: 4px 8px;"
        "  border-radius: 3px;"
        "}"
        "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
        "  background-color: %4;"
        "  color: %6;"
        "}"
    ).arg(global_theme.input, global_theme.foreground, global_theme.border,
          global_theme.primary, global_theme.card, global_theme.primary_foreground);
}

inline void style_combobox(QComboBox *combo) {
    if (!combo) return;
    if (!combo->itemDelegate() || combo->itemDelegate()->metaObject()->className() == QString("QItemDelegate")) {
        combo->setItemDelegate(new QStyledItemDelegate(combo));
    }
    combo->setStyleSheet(get_combobox_qss());
}

inline void load_theme(QString &path){
    QSettings global_values(QCoreApplication::applicationDirPath() + "/styles/"+path+"/global_values.ini", QSettings::IniFormat);
    global_theme.background = global_values.value("colors/background").toString();
    global_theme.foreground = global_values.value("colors/foreground").toString();
    global_theme.card = global_values.value("colors/card").toString();
    global_theme.card_foreground = global_values.value("colors/card_foreground").toString();
    global_theme.popover = global_values.value("colors/popover").toString();
    global_theme.popover_foreground = global_values.value("colors/popover_foreground").toString();
    global_theme.primary = global_values.value("colors/primary").toString();
    global_theme.primary_foreground = global_values.value("colors/primary_foreground").toString();
    global_theme.secondary = global_values.value("colors/secondary").toString();
    global_theme.secondary_foreground = global_values.value("colors/secondary_foreground").toString();
    global_theme.muted = global_values.value("colors/muted").toString();
    global_theme.muted_foreground = global_values.value("colors/muted_foreground").toString();
    global_theme.accent = global_values.value("colors/accent").toString();
    global_theme.accent_foreground = global_values.value("colors/accent_foreground").toString();
    global_theme.destructive = global_values.value("colors/destructive").toString();
    global_theme.destructive_foreground = global_values.value("colors/destructive_foreground").toString();
    global_theme.border = global_values.value("colors/border").toString();
    global_theme.input = global_values.value("colors/input").toString();
    global_theme.ring = global_values.value("colors/ring").toString();
    global_theme.sidebar = global_values.value("colors/sidebar").toString();
    global_theme.sidebar_foreground = global_values.value("colors/sidebar_foreground").toString();
    global_theme.sidebar_primary = global_values.value("colors/sidebar_primary").toString();
    global_theme.sidebar_primary_foreground = global_values.value("colors/sidebar_primary_foreground").toString();
    global_theme.sidebar_accent = global_values.value("colors/sidebar_accent").toString();
    global_theme.sidebar_accent_foreground = global_values.value("colors/sidebar_accent_foreground").toString();
    global_theme.sidebar_border = global_values.value("colors/sidebar_border").toString();
    global_theme.sidebar_ring = global_values.value("colors/sidebar_ring").toString();
    global_theme.close = global_values.value("colors/close").toString();
}

inline QString get_title_bar_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/title_bar.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        qss.replace("${close}", global_theme.close);
        return qss;
    }else{
        return "";
    }
}

inline QString get_central_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/central.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${background}", global_theme.background);
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${border}", global_theme.border);
        qss.replace("${card}", global_theme.card);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${muted}", global_theme.muted);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        qss.replace("${input}", global_theme.input);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${popover_foreground}", global_theme.popover_foreground);
        qss.replace("${destructive}", global_theme.destructive);
        qss.replace("${destructive_foreground}", global_theme.destructive_foreground);
        return qss;
    }else{
        return "";
    }
}

inline QString get_side_bar_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/side_bar.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${input}", global_theme.input);
        qss.replace("${ring}", global_theme.ring);
        qss.replace("${border}", global_theme.border);
        qss.replace("${card}", global_theme.card);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${destructive}", global_theme.destructive);
        qss.replace("${destructive_foreground}", global_theme.destructive_foreground);
        return qss;
    }else{
        return "";
    }
}

inline QString get_main_area_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/main_area.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${muted}", global_theme.muted);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${background}", global_theme.background);
        qss.replace("${border}", global_theme.border);
        qss.replace("${input}", global_theme.input);
        qss.replace("${card}", global_theme.card);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${destructive}", global_theme.destructive);
        qss.replace("${destructive_foreground}", global_theme.destructive_foreground);
        return qss;
    }else{
        return "";
    }
}

inline QString get_chat_area_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/chat_area.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${muted}", global_theme.muted);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${background}", global_theme.background);
        qss.replace("${border}", global_theme.border);
        qss.replace("${input}", global_theme.input);
        qss.replace("${card}", global_theme.card);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${ring}", global_theme.ring);
        return qss;
    }else{
        return "";
    }
}

inline QString get_popup_qss(QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/popup.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${background}", global_theme.background);
        qss.replace("${card}", global_theme.card);
        qss.replace("${input}", global_theme.input);
        qss.replace("${ring}", global_theme.ring);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${border}", global_theme.border);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${muted}", global_theme.muted);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        qss.replace("${destructive}", global_theme.destructive);
        qss.replace("${destructive_foreground}", global_theme.destructive_foreground);
        return qss;
    }else{
        return "";
    }
}

inline QString get_roles_popup_qss(const QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/roles_popup.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${background}", global_theme.background);
        qss.replace("${input}", global_theme.input);
        qss.replace("${ring}", global_theme.ring);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${primary_foreground}", global_theme.primary_foreground);
        qss.replace("${border}", global_theme.border);
        qss.replace("${card}", global_theme.card);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${destructive}", global_theme.destructive);
        qss.replace("${destructive_foreground}", global_theme.destructive_foreground);
        qss.replace("${muted}", global_theme.muted);
        qss.replace("${muted_foreground}", global_theme.muted_foreground);
        return qss;
    }
    return "";
}

inline QString get_toast_qss(const QString &path){
    QFile file(QCoreApplication::applicationDirPath() + "/styles/"+path+"/toast.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("${foreground}", global_theme.foreground);
        qss.replace("${popover}", global_theme.popover);
        qss.replace("${border}", global_theme.border);
        qss.replace("${primary}", global_theme.primary);
        qss.replace("${destructive}", global_theme.destructive);
        return qss;
    }
    return QString(
        "QWidget#toast_container { background: transparent; border: none; }"
        "QWidget#toast_card, QWidget#toast_card_info, QWidget#toast_card_success, QWidget#toast_card_error {"
        "  background-color: %1; border: 1px solid %2; border-radius: 6px;"
        "}"
        "QWidget#toast_card_error, QWidget#toast_card[toast_type=\"error\"] { border: 1px solid %3; }"
        "QWidget#toast_card_success, QWidget#toast_card[toast_type=\"success\"] { border: 1px solid %4; }"
        "QWidget#toast_card QLabel, QWidget#toast_card_info QLabel, QWidget#toast_card_success QLabel, QWidget#toast_card_error QLabel {"
        "  background: transparent; color: %5; font-size: 13px; font-weight: 500;"
        "}"
        "QWidget#toast_card_error QLabel, QWidget#toast_card[toast_type=\"error\"] QLabel { color: %3; }"
        "QWidget#toast_card_success QLabel, QWidget#toast_card[toast_type=\"success\"] QLabel { color: %4; }"
        "QLabel#toast_icon { font-size: 14px; font-weight: bold; }"
    ).arg(global_theme.popover, global_theme.border, global_theme.destructive, global_theme.primary, global_theme.foreground);
}

inline QString get_emoji_picker_qss(const QString &path) {
    Q_UNUSED(path);
    return QString(
        "QFrame#emoji_picker_popup {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
        "QLabel#popup_section_title {"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  padding-left: 2px;"
        "  color: %3;"
        "  background: transparent;"
        "}"
        "QScrollArea, QScrollArea > QWidget, QScrollArea > QWidget > QWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-size: 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %4;"
        "}"
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: transparent;"
        "  width: 6px;"
        "  margin: 0px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: %2;"
        "  min-height: 20px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background-color: %5;"
        "}"
    ).arg(global_theme.card, global_theme.border, global_theme.muted_foreground,
          global_theme.popover, global_theme.primary);
}

inline QPixmap colored_svg(const QString &path, const QColor &color, const QSize &size){
    QSvgRenderer renderer(path);

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);

    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);

    return pixmap;
}

inline QPixmap colored_png(const QString &path, const QColor &color, const QSize &size){
    QPixmap pixmap(path);

    if (pixmap.isNull()) return {};

    pixmap = pixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);

    return pixmap;
}

inline QPixmap crop_avatar_cover(const QPixmap &src, const QSize &target_size, int radius = 0){
    if (src.isNull() || target_size.isEmpty()) return {};

    QPixmap scaled_pix = src.scaled(target_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    int crop_x = qMax(0, (scaled_pix.width() - target_size.width()) / 2);
    int crop_y = qMax(0, (scaled_pix.height() - target_size.height()) / 2);
    QPixmap cropped = scaled_pix.copy(crop_x, crop_y, target_size.width(), target_size.height());

    if (radius <= 0) return cropped;

    QPixmap result(target_size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    if (radius >= target_size.width() / 2 && radius >= target_size.height() / 2) {
        path.addEllipse(0, 0, target_size.width(), target_size.height());
    } else {
        path.addRoundedRect(0, 0, target_size.width(), target_size.height(), radius, radius);
    }
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);
    return result;
}

#endif // THEME_H
