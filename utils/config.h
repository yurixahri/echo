#ifndef CONFIG_H
#define CONFIG_H

#include <QFileInfo>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QSettings>
// #include "utils/stringUtils.h"

typedef struct {
    QString address;
    bool is_ssl;
    QString uuid;
    QString password;
} server_t;


typedef struct {
    QString username;
    bool is_ssl;
    QList<server_t> server_list;
    QReadWriteLock lock;
} config_t;

extern config_t config;

bool write_config();
bool get_config();



#endif // CONFIG_H
