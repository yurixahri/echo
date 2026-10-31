#ifndef CONFIG_H
#define CONFIG_H

#include <QFileInfo>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QSettings>
#include <QMap>
// #include "utils/stringUtils.h"

typedef struct {
    QString address;
    bool is_ssl;
    QString uuid;
    QString password;
    QMap<uint64_t, QString> room_passwords;
    QMap<uint64_t, int> user_volumes;
} server_t;


typedef struct {
    QString username;
    bool is_ssl;
    int voice_bitrate_kbps = 40;
    int input_volume = 100;
    int output_volume = 100;
    bool vad_enabled = false;
    int vad_threshold_db = -40;
    QString theme = "default";
    bool sfx_enabled = true;
    int sfx_volume = 100;
    QList<server_t> server_list;
    QMap<uint64_t, int> user_volumes;
    int window_width = 1050;
    int window_height = 680;
    bool window_maximized = false;
    QByteArray splitter_state;
    QReadWriteLock lock;
} config_t;

extern config_t config;

bool write_config();
void write_config_async();
bool get_config();



#endif // CONFIG_H
