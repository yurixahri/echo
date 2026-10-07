#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QSettings>
#include <QSvgRenderer>
#include <QPainter>
#include <QFile>

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
        return qss;
    }else{
        return "";
    }
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

#endif // THEME_H
