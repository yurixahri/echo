#ifndef SOUND_H
#define SOUND_H

#include <QSoundEffect>
#include <QUrl>
#include <QFile>
#include <QCoreApplication>
#include "utils/config.h"

struct sound_manager_t {
    bool initialized = false;
    QSoundEffect sfx_join;
    QSoundEffect sfx_leave;
    QSoundEffect sfx_poke;
    QSoundEffect sfx_msg[3];
    int msg_index = 0;
};

inline sound_manager_t &get_sound_manager() {
    static sound_manager_t mgr;
    return mgr;
}

inline QUrl sound_file_url(const QString &base_name) {
    QString sounds_dir = QCoreApplication::applicationDirPath() + "/assets/sounds/";
    if (QFile::exists(sounds_dir + base_name + ".wav")) {
        return QUrl::fromLocalFile(sounds_dir + base_name + ".wav");
    }
    return QUrl::fromLocalFile(sounds_dir + base_name + ".ogg");
}

inline void sound_init() {
    sound_manager_t &mgr = get_sound_manager();
    if (mgr.initialized) return;

    mgr.sfx_join.setSource(sound_file_url("voice_join"));
    mgr.sfx_leave.setSource(sound_file_url("voice_leave"));
    mgr.sfx_poke.setSource(sound_file_url("poke"));

    for (int i = 0; i < 3; ++i) {
        mgr.sfx_msg[i].setSource(sound_file_url("message"));
    }

    float vol = qBound(0.0f, (float)config.sfx_volume / 100.0f, 1.0f);
    mgr.sfx_join.setVolume(vol);
    mgr.sfx_leave.setVolume(vol);
    mgr.sfx_poke.setVolume(vol);
    for (int i = 0; i < 3; ++i) {
        mgr.sfx_msg[i].setVolume(vol);
    }

    mgr.initialized = true;
}

inline void sound_update_volume(int vol_percent) {
    sound_manager_t &mgr = get_sound_manager();
    if (!mgr.initialized) sound_init();
    float vol = qBound(0.0f, (float)vol_percent / 100.0f, 1.0f);
    mgr.sfx_join.setVolume(vol);
    mgr.sfx_leave.setVolume(vol);
    mgr.sfx_poke.setVolume(vol);
    for (int i = 0; i < 3; ++i) {
        mgr.sfx_msg[i].setVolume(vol);
    }
}

inline void sound_play_voice_join() {
    if (!config.sfx_enabled) return;
    sound_manager_t &mgr = get_sound_manager();
    if (!mgr.initialized) sound_init();
    // if (mgr.sfx_join.isPlaying()) return;
    mgr.sfx_join.play();
}

inline void sound_play_voice_leave() {
    if (!config.sfx_enabled) return;
    sound_manager_t &mgr = get_sound_manager();
    if (!mgr.initialized) sound_init();
    // if (mgr.sfx_leave.isPlaying()) return;
    mgr.sfx_leave.play();
}

inline void sound_play_poke() {
    if (!config.sfx_enabled) return;
    sound_manager_t &mgr = get_sound_manager();
    if (!mgr.initialized) sound_init();
    if (mgr.sfx_poke.isPlaying()) return;
    mgr.sfx_poke.play();
}

inline void sound_play_message() {
    if (!config.sfx_enabled) return;
    sound_manager_t &mgr = get_sound_manager();
    if (!mgr.initialized) sound_init();
    mgr.sfx_msg[mgr.msg_index].play();
    mgr.msg_index = (mgr.msg_index + 1) % 3;
}

#endif // SOUND_H
