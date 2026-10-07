#include "utils/config.h"
#include <QCoreApplication>

config_t config;

bool write_config(){
    QReadLocker locker(&config.lock);

    QString path = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(path, QSettings::IniFormat);

    settings.setValue("settings/username", config.username);
    settings.setValue("settings/is_ssl", config.is_ssl);

    settings.remove("servers");

    for (qsizetype i = 0; i < config.server_list.size(); ++i) {
       QString group = "servers/server" + QString::number(i + 1);

       settings.beginGroup(group);

       settings.setValue("address", config.server_list[i].address);
       settings.setValue("is_ssl", config.server_list[i].is_ssl);
       settings.setValue("uuid", config.server_list[i].uuid);
       settings.setValue("password", config.server_list[i].password);

       settings.endGroup();
    }

    settings.sync();

    if (settings.status() != QSettings::NoError) {
       qDebug() << "Failed writing config:" << settings.status();
       return false;
    }

    return true;
}

bool get_config(){
    QString path = QCoreApplication::applicationDirPath() + "/config.ini";

    if (!QFileInfo::exists(path)) {
        qDebug() << "Couldn't find config file, exported default config";

        {
            QWriteLocker locker(&config.lock);
            config.username = "Anonymous";
            config.is_ssl = true;
            config.server_list.clear();
        }
        return write_config();
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        qDebug() << "Couldn't read config:" << settings.status();
        return false;
    }

    QWriteLocker locker(&config.lock);

    config.username = settings.value("settings/username", "Anonymous").toString();
    config.is_ssl = settings.value("settings/is_ssl", true).toBool();

    config.server_list.clear();

    settings.beginGroup("servers");

    for (const QString &server : settings.childGroups()) {
        settings.beginGroup(server);

        server_t server_temp{};
        server_temp.address = settings.value("address").toString();
        server_temp.is_ssl = settings.value("is_ssl").toBool();
        server_temp.uuid = settings.value("uuid").toString();
        server_temp.password = settings.value("password").toString();

        config.server_list.append(server_temp);

        settings.endGroup();
    }

    settings.endGroup();

    return true;
}


