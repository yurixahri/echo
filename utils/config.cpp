#include "utils/config.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <atomic>

config_t config;

static QMutex s_config_file_mutex;
static std::atomic<uint64_t> s_config_write_gen{0};

struct config_snapshot_t {
    QString username;
    bool is_ssl = true;
    int voice_bitrate_kbps = 40;
    int input_volume = 100;
    int output_volume = 100;
    bool vad_enabled = false;
    int vad_threshold_db = -40;
    QString theme = "default";
    bool sfx_enabled = true;
    int sfx_volume = 100;
    int window_width = 1050;
    int window_height = 680;
    bool window_maximized = false;
    QByteArray splitter_state;
    QList<server_t> server_list;
};

static config_snapshot_t get_config_snapshot() {
    QReadLocker locker(&config.lock);
    config_snapshot_t s;
    s.username = config.username;
    s.is_ssl = config.is_ssl;
    s.voice_bitrate_kbps = config.voice_bitrate_kbps;
    s.input_volume = config.input_volume;
    s.output_volume = config.output_volume;
    s.vad_enabled = config.vad_enabled;
    s.vad_threshold_db = config.vad_threshold_db;
    s.theme = config.theme;
    s.sfx_enabled = config.sfx_enabled;
    s.sfx_volume = config.sfx_volume;
    s.window_width = config.window_width;
    s.window_height = config.window_height;
    s.window_maximized = config.window_maximized;
    s.splitter_state = config.splitter_state;
    s.server_list = config.server_list;
    return s;
}

static bool write_config_snapshot_unlocked(const config_snapshot_t &s) {
    QString path = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(path, QSettings::IniFormat);

    settings.setValue("settings/username", s.username);
    settings.setValue("settings/is_ssl", s.is_ssl);
    settings.setValue("settings/voice_bitrate_kbps", s.voice_bitrate_kbps);
    settings.setValue("settings/input_volume", s.input_volume);
    settings.setValue("settings/output_volume", s.output_volume);
    settings.setValue("settings/vad_enabled", s.vad_enabled);
    settings.setValue("settings/vad_threshold_db", s.vad_threshold_db);
    settings.setValue("settings/theme", s.theme);
    settings.setValue("settings/sfx_enabled", s.sfx_enabled);
    settings.setValue("settings/sfx_volume", s.sfx_volume);

    settings.setValue("window/width", s.window_width);
    settings.setValue("window/height", s.window_height);
    settings.setValue("window/maximized", s.window_maximized);
    settings.setValue("window/splitter_state", s.splitter_state.toHex());

    settings.remove("servers");

    for (qsizetype i = 0; i < s.server_list.size(); ++i) {
       QString group = "servers/server" + QString::number(i + 1);

       settings.beginGroup(group);

       settings.setValue("address", s.server_list[i].address);
       settings.setValue("is_ssl", s.server_list[i].is_ssl);
       settings.setValue("uuid", s.server_list[i].uuid);
       settings.setValue("password", s.server_list[i].password);

       settings.beginGroup("room_passwords");
       for (auto it = s.server_list[i].room_passwords.begin(); it != s.server_list[i].room_passwords.end(); ++it) {
           settings.setValue(QString::number(it.key()), it.value());
       }
       settings.endGroup();

       settings.beginGroup("user_volumes");
       for (auto it = s.server_list[i].user_volumes.begin(); it != s.server_list[i].user_volumes.end(); ++it) {
            if (it.value() != 100){
                settings.setValue(QString::number(it.key()), it.value());
            }
       }
       settings.endGroup();

       settings.endGroup();
    }

    settings.sync();

    if (settings.status() != QSettings::NoError) {
       // qDebug() << "Failed writing config:" << settings.status();
       return false;
    }

    return true;
}

bool write_config(){
    s_config_write_gen++;
    QMutexLocker file_lock(&s_config_file_mutex);
    return write_config_snapshot_unlocked(get_config_snapshot());
}

// ponytail: generation check coalesces rapid writes; upgrade to QTimer debouncer if threadpool dispatch overhead ever matters.
void write_config_async(){
    uint64_t gen = ++s_config_write_gen;
    QThreadPool::globalInstance()->start([gen]() {
        if (gen < s_config_write_gen.load()) return;
        QMutexLocker file_lock(&s_config_file_mutex);
        if (gen < s_config_write_gen.load()) return;
        write_config_snapshot_unlocked(get_config_snapshot());
    });
}

bool get_config(){
    QString path = QCoreApplication::applicationDirPath() + "/config.ini";

    if (!QFileInfo::exists(path)) {
        // qDebug() << "Couldn't find config file, exported default config";

        {
            QWriteLocker locker(&config.lock);
            config.username = "Anonymous";
            config.is_ssl = true;
            config.voice_bitrate_kbps = 40;
            config.input_volume = 100;
            config.output_volume = 100;
            config.vad_enabled = false;
            config.vad_threshold_db = -40;
            config.theme = "default";
            config.sfx_enabled = true;
            config.sfx_volume = 100;
            config.server_list.clear();
        }
        return write_config();
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        // qDebug() << "Couldn't read config:" << settings.status();
        return false;
    }

    QWriteLocker locker(&config.lock);

    config.username = settings.value("settings/username", "Anonymous").toString();
    config.is_ssl = settings.value("settings/is_ssl", true).toBool();
    config.voice_bitrate_kbps = qBound(
        16, settings.value("settings/voice_bitrate_kbps", 40).toInt(), 64);
    config.input_volume = qBound(0, settings.value("settings/input_volume", 100).toInt(), 200);
    config.output_volume = qBound(0, settings.value("settings/output_volume", 100).toInt(), 150);
    config.vad_enabled = settings.value("settings/vad_enabled", false).toBool();
    config.vad_threshold_db = qBound(-80, settings.value("settings/vad_threshold_db", -40).toInt(), -10);
    config.theme = settings.value("settings/theme", "default").toString();
    config.sfx_enabled = settings.value("settings/sfx_enabled", true).toBool();
    config.sfx_volume = qBound(0, settings.value("settings/sfx_volume", 100).toInt(), 100);

    config.window_width = qMax(400, settings.value("window/width", 1050).toInt());
    config.window_height = qMax(300, settings.value("window/height", 680).toInt());
    config.window_maximized = settings.value("window/maximized", false).toBool();
    config.splitter_state = QByteArray::fromHex(settings.value("window/splitter_state").toByteArray());

    config.server_list.clear();

    settings.beginGroup("servers");

    for (const QString &server : settings.childGroups()) {
        settings.beginGroup(server);

        server_t server_temp{};
        server_temp.address = settings.value("address").toString();
        server_temp.is_ssl = settings.value("is_ssl").toBool();
        server_temp.uuid = settings.value("uuid").toString();
        server_temp.password = settings.value("password").toString();

        settings.beginGroup("room_passwords");
        for (const QString &key : settings.childKeys()) {
            server_temp.room_passwords.insert(key.toULongLong(), settings.value(key).toString());
        }
        settings.endGroup();

        settings.beginGroup("user_volumes");
        for (const QString &key : settings.childKeys()) {
            server_temp.user_volumes.insert(key.toULongLong(), qBound(0, settings.value(key, 100).toInt(), 200));
        }
        settings.endGroup();

        config.server_list.append(server_temp);

        settings.endGroup();
    }

    settings.endGroup();

    return true;
}

