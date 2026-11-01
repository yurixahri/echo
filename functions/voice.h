#ifndef CLIENT_VOICE_H
#define CLIENT_VOICE_H

#include <QUdpSocket>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QtEndian>
#include <QtMath>
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QNetworkDatagram>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QThread>
#include <QMetaObject>
#include <QCoreApplication>
#include <QDebug>
#include <opus/opus.h>
#include <functional>
#include <utility>
#include <cstring>
#include <cmath>
#include "common/types.h"
#include "utils/asn1_parser.h"
#include "utils/config.h"
#include "functions/room.h"
#include "functions/sound.h"

struct voice_client_t {
    QAudioSource *audio_source = nullptr;
    QIODevice *input_stream = nullptr;
    QAudioSink *audio_sink = nullptr;
    QIODevice *output_stream = nullptr;
    QTimer *playback_timer = nullptr;
    QTimer *ping_timer = nullptr;
    OpusEncoder *encoder = nullptr;
    int bitrate_kbps = 40;
    int input_volume = 100;
    int output_volume = 100;
    bool vad_enabled = false;
    int vad_threshold_db = -40;

    uint64_t current_room_id = 0;
    uint64_t user_id = 0;
    QString current_room_name;
    bool is_mute = false;
    bool is_deaf = false;
    uint32_t tx_seq = 0;
    QString server_host;
    QHostAddress server_address;
    quint16 server_port = 5555;
    uint64_t start_generation = 0;
    bool udp_ready = false;
    QByteArray input_device_id;
    QByteArray output_device_id;
    QByteArray pending_pcm;
    struct audio_peer_t {
        QByteArray pcm;
        QQueue<QByteArray> encoded_packets;
        qint64 last_packet_at = 0;
        qint64 last_voice_activity_at = 0;
        OpusDecoder *decoder = nullptr;
        bool playback_started = false;
    };
    QMap<uint64_t, audio_peer_t> incoming_audio;
    QSet<uint64_t> active_speakers;
    qint64 last_local_voice_activity_at = 0;
};

inline voice_client_t voice_client;

struct voice_ui_state_t {
    uint64_t current_room_id = 0;
    QString current_room_name;
    bool is_mute = false;
    bool is_deaf = false;
    int bitrate_kbps = 40;
    int input_volume = 100;
    int output_volume = 100;
    bool vad_enabled = false;
    int vad_threshold_db = -40;
    QByteArray input_device_id;
    QByteArray output_device_id;
    QSet<uint64_t> active_speakers;
};

inline voice_ui_state_t voice_ui_state;

inline int voice_get_user_volume(uint64_t user_id) {
    if (client.server.user_volumes.contains(user_id)) {
        return client.server.user_volumes.value(user_id, 100);
    }
    QReadLocker lock(&config.lock);
    for (const server_t &s : config.server_list) {
        if (s.address == client.server.address) {
            return s.user_volumes.value(user_id, 100);
        }
    }
    return 100;
}
inline void voice_set_user_volume(uint64_t user_id, int vol) {
    int bounded = qBound(0, vol, 200);
    client.server.user_volumes[user_id] = bounded;
    {
        QWriteLocker lock(&config.lock);
        for (server_t &s : config.server_list) {
            if (s.address == client.server.address) {
                s.user_volumes[user_id] = bounded;
                break;
            }
        }
    }
    write_config_async();
}

inline std::function<void(int level_0_to_100, double db)> on_voice_mic_test_level = nullptr;
inline bool voice_mic_test_active = false;

struct voice_worker_t {
    QThread thread;
    QObject *context = nullptr;

    voice_worker_t() {
        context = new QObject();
        context->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, context, &QObject::deleteLater);
        thread.start();
        thread.setPriority(QThread::HighPriority);
    }

    ~voice_worker_t() {
        thread.quit();
        thread.wait();
    }
};

inline voice_worker_t &voice_worker() {
    static voice_worker_t worker;
    return worker;
}

inline bool voice_post(std::function<void()> task) {
    QObject *context = voice_worker().context;
    bool queued = QMetaObject::invokeMethod(
        context, std::move(task), Qt::QueuedConnection);
    if (!queued) qWarning() << "Could not queue voice task.";
    return queued;
}

struct voice_udp_worker_t {
    QThread thread;
    QObject *context = nullptr;
    QUdpSocket *socket = nullptr;
    QMutex incoming_mutex;
    QQueue<QPair<QByteArray, uint64_t>> incoming_datagrams;
    bool incoming_drain_scheduled = false;

    voice_udp_worker_t() {
        context = new QObject();
        context->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, context, &QObject::deleteLater);
        thread.start();
    }

    ~voice_udp_worker_t() {
        if (thread.isRunning()) {
            QMetaObject::invokeMethod(context, [this]() {
                if (socket) {
                    socket->close();
                    delete socket;
                    socket = nullptr;
                }
            }, Qt::BlockingQueuedConnection);
            thread.quit();
            thread.wait();
        }
    }
};

inline voice_udp_worker_t &voice_udp_worker() {
    static voice_udp_worker_t worker;
    return worker;
}

inline std::function<void(bool active, uint64_t room_id, const QString &room_name)> on_voice_state_changed = nullptr;
inline std::function<void(const QSet<uint64_t> &user_ids)> on_active_voice_speakers_changed = nullptr;
constexpr int VOICE_MAX_AUDIO_PAYLOAD = 1200;
constexpr int VOICE_AUDIO_SAMPLE_RATE = 48000;
constexpr int VOICE_AUDIO_FRAME_DURATION_MS = 10;
constexpr int VOICE_AUDIO_FRAME_SAMPLES =
    VOICE_AUDIO_SAMPLE_RATE * VOICE_AUDIO_FRAME_DURATION_MS / 1000;
constexpr int VOICE_AUDIO_FRAME_SIZE = VOICE_AUDIO_FRAME_SAMPLES * 2;
constexpr int VOICE_AUDIO_BUFFER_SIZE = VOICE_AUDIO_FRAME_SIZE * 20;

inline void voice_handle_datagram(const QByteArray &data, uint64_t generation);
inline void voice_udp_drain_pending();
inline void voice_stop_channel_impl(bool notify_server);
inline void voice_fail_start_channel(
    uint64_t room_id, uint64_t start_generation, const QString &error);

inline bool voice_udp_open(uint64_t generation, QString &error) {
    voice_udp_worker_t &worker = voice_udp_worker();
    bool opened = false;
    bool invoked = QMetaObject::invokeMethod(
        worker.context, [&worker, generation, &error, &opened]() {
            if (worker.socket) {
                worker.socket->close();
                delete worker.socket;
                worker.socket = nullptr;
            }

            auto *socket = new QUdpSocket(worker.context);
            if (!socket->bind(QHostAddress::Any, 0)) {
                error = socket->errorString();
                delete socket;
                return;
            }

            worker.socket = socket;
            QObject::connect(socket, &QUdpSocket::readyRead, socket,
                [socket, generation, &worker]() {
                    while (socket->hasPendingDatagrams()) {
                        QByteArray data = socket->receiveDatagram().data();
                        bool schedule_drain = false;
                        {
                            QMutexLocker locker(&worker.incoming_mutex);
                            constexpr int max_pending_datagrams = 32;
                            while (worker.incoming_datagrams.size() >=
                                   max_pending_datagrams) {
                                worker.incoming_datagrams.dequeue();
                            }
                            worker.incoming_datagrams.enqueue(
                                qMakePair(data, generation));
                            if (!worker.incoming_drain_scheduled) {
                                worker.incoming_drain_scheduled = true;
                                schedule_drain = true;
                            }
                        }
                        if (schedule_drain &&
                            !voice_post([]() { voice_udp_drain_pending(); })) {
                            QMutexLocker locker(&worker.incoming_mutex);
                            worker.incoming_drain_scheduled = false;
                            qWarning() << "Could not queue voice datagram processing.";
                        }
                    }
                });
            opened = true;
        }, Qt::BlockingQueuedConnection);

    if (!invoked && error.isEmpty()) {
        error = "Could not initialize voice UDP worker.";
    }
    return invoked && opened;
}

inline void voice_udp_drain_pending() {
    voice_udp_worker_t &worker = voice_udp_worker();
    while (true) {
        QList<QPair<QByteArray, uint64_t>> batch;
        {
            QMutexLocker locker(&worker.incoming_mutex);
            constexpr int datagrams_per_batch = 16;
            while (!worker.incoming_datagrams.isEmpty() &&
                   batch.size() < datagrams_per_batch) {
                batch.append(worker.incoming_datagrams.dequeue());
            }
            if (batch.isEmpty()) {
                worker.incoming_drain_scheduled = false;
                return;
            }
        }
        for (const auto &datagram : batch) {
            voice_handle_datagram(datagram.first, datagram.second);
        }
    }
}

inline void voice_udp_send_packet(const QByteArray &packet,
                                  const QHostAddress &address,
                                  quint16 port) {
    voice_udp_worker_t &worker = voice_udp_worker();
    bool queued = QMetaObject::invokeMethod(
        worker.context, [&worker, packet, address, port]() {
            if (!worker.socket ||
                worker.socket->state() != QAbstractSocket::BoundState) {
                return;
            }
            qint64 written = worker.socket->writeDatagram(packet, address, port);
            if (written != packet.size()) {
                qWarning() << "Voice UDP send failed:"
                           << worker.socket->errorString();
            }
        }, Qt::QueuedConnection);
    if (!queued) qWarning() << "Could not queue voice UDP packet.";
}

inline void voice_udp_close() {
    voice_udp_worker_t &worker = voice_udp_worker();
    if (!worker.thread.isRunning()) return;
    bool invoked = QMetaObject::invokeMethod(
        worker.context, [&worker]() {
            if (!worker.socket) return;
            worker.socket->close();
            delete worker.socket;
            worker.socket = nullptr;
        }, Qt::BlockingQueuedConnection);
    if (!invoked) qWarning() << "Could not close voice UDP socket.";
}

inline void voice_udp_shutdown() {
    voice_udp_close();
    voice_udp_worker_t &worker = voice_udp_worker();
    if (!worker.thread.isRunning()) return;
    worker.thread.quit();
    worker.thread.wait();
}

inline void voice_send_packet(uint8_t packet_type, const QByteArray &payload = QByteArray());
inline void voice_clear_incoming_audio();
inline void voice_start_channel_impl(
    uint64_t room_id, const QString &room_name, const QString &server_entry,
    uint64_t user_id);

inline void voice_emit_state_changed(bool active, uint64_t room_id, const QString &room_name) {
    bool is_mute = voice_client.is_mute;
    bool is_deaf = voice_client.is_deaf;
    QCoreApplication *app = QCoreApplication::instance();
    if (!app || !QMetaObject::invokeMethod(
            app, [active, room_id, room_name, is_mute, is_deaf]() {
                voice_ui_state.current_room_id = room_id;
                voice_ui_state.current_room_name = room_name;
                voice_ui_state.is_mute = is_mute;
                voice_ui_state.is_deaf = is_deaf;
                if (on_voice_state_changed) {
                    on_voice_state_changed(active, room_id, room_name);
                }
            }, Qt::QueuedConnection)) {
        qWarning() << "Could not queue voice state update.";
    }
}

inline void voice_emit_active_speakers(const QSet<uint64_t> &user_ids) {
    QCoreApplication *app = QCoreApplication::instance();
    if (!app || !QMetaObject::invokeMethod(
            app, [user_ids]() {
                voice_ui_state.active_speakers = user_ids;
                if (on_active_voice_speakers_changed) {
                    on_active_voice_speakers_changed(user_ids);
                }
            }, Qt::QueuedConnection)) {
        qWarning() << "Could not queue voice speaker update.";
    }
}

inline void voice_request_leave_room(uint64_t room_id) {
    QCoreApplication *app = QCoreApplication::instance();
    if (!app || !QMetaObject::invokeMethod(
            app, [room_id]() { user_leave_room(room_id, 1); },
            Qt::QueuedConnection)) {
        qWarning() << "Could not queue voice room leave request.";
    }
}

inline void voice_codec_initialize(int bitrate_kbps) {
    if (voice_client.encoder) {
        opus_encoder_destroy(voice_client.encoder);
        voice_client.encoder = nullptr;
    }

    int opus_error = OPUS_OK;
    voice_client.encoder = opus_encoder_create(
        VOICE_AUDIO_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opus_error);
    if (!voice_client.encoder || opus_error != OPUS_OK) {
        qWarning() << "Could not create Opus encoder:" << opus_strerror(opus_error);
        voice_client.encoder = nullptr;
        return;
    }

    int ctl_error = opus_encoder_ctl(
        voice_client.encoder, OPUS_SET_BITRATE(bitrate_kbps * 1000));
    if (ctl_error == OPUS_OK) ctl_error = opus_encoder_ctl(voice_client.encoder, OPUS_SET_COMPLEXITY(5));
    if (ctl_error == OPUS_OK) ctl_error = opus_encoder_ctl(voice_client.encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    if (ctl_error == OPUS_OK) ctl_error = opus_encoder_ctl(voice_client.encoder, OPUS_SET_INBAND_FEC(1));
    if (ctl_error == OPUS_OK) ctl_error = opus_encoder_ctl(voice_client.encoder, OPUS_SET_PACKET_LOSS_PERC(10));
    if (ctl_error != OPUS_OK) {
        qWarning() << "Could not configure Opus encoder:" << opus_strerror(ctl_error);
        opus_encoder_destroy(voice_client.encoder);
        voice_client.encoder = nullptr;
    }
}

inline void voice_update_local_speaker(bool voice_activity_detected, qint64 now) {
    uint64_t user_id = voice_client.user_id;
    if (user_id == 0) return;

    bool changed = false;
    if (voice_activity_detected && !voice_client.is_mute && voice_client.current_room_id > 0) {
        voice_client.last_local_voice_activity_at = now;
        if (!voice_client.active_speakers.contains(user_id)) {
            voice_client.active_speakers.insert(user_id);
            changed = true;
        }
    } else if (voice_client.is_mute || voice_client.current_room_id == 0 ||
               (voice_client.active_speakers.contains(user_id) &&
                now - voice_client.last_local_voice_activity_at > 250)) {
        voice_client.last_local_voice_activity_at = 0;
        changed = voice_client.active_speakers.remove(user_id) > 0;
    }

    if (changed) voice_emit_active_speakers(voice_client.active_speakers);
}

inline void voice_set_bitrate_impl(int bitrate_kbps, bool persist) {
    voice_client.bitrate_kbps = qBound(16, bitrate_kbps, 128);
    if (voice_client.encoder) {
        int error = opus_encoder_ctl(
            voice_client.encoder, OPUS_SET_BITRATE(voice_client.bitrate_kbps * 1000));
        if (error != OPUS_OK) {
            qWarning() << "Could not update Opus bitrate:" << opus_strerror(error);
        }
    }

    {
        QWriteLocker locker(&config.lock);
        config.voice_bitrate_kbps = voice_client.bitrate_kbps;
    }
    if (persist) {
        write_config_async();
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        QMetaObject::invokeMethod(app, [bitrate_kbps = voice_client.bitrate_kbps]() {
            voice_ui_state.bitrate_kbps = bitrate_kbps;
        }, Qt::QueuedConnection);
    }
}

inline void voice_set_input_volume(int volume_percent, bool persist = true) {
    voice_client.input_volume = qBound(0, volume_percent, 200);
    {
        QWriteLocker locker(&config.lock);
        config.input_volume = voice_client.input_volume;
    }
    if (persist) {
        write_config_async();
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        QMetaObject::invokeMethod(app, [vol = voice_client.input_volume]() {
            voice_ui_state.input_volume = vol;
        }, Qt::QueuedConnection);
    }
}

inline void voice_set_output_volume(int volume_percent, bool persist = true) {
    voice_client.output_volume = qBound(0, volume_percent, 150);
    if (voice_client.audio_sink) {
        voice_client.audio_sink->setVolume(voice_client.output_volume / 100.0);
    }
    {
        QWriteLocker locker(&config.lock);
        config.output_volume = voice_client.output_volume;
    }
    if (persist) {
        write_config_async();
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        QMetaObject::invokeMethod(app, [vol = voice_client.output_volume]() {
            voice_ui_state.output_volume = vol;
        }, Qt::QueuedConnection);
    }
}

inline void voice_set_vad_enabled(bool enabled, bool persist = true) {
    voice_client.vad_enabled = enabled;
    {
        QWriteLocker locker(&config.lock);
        config.vad_enabled = enabled;
    }
    if (persist) {
        write_config_async();
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        QMetaObject::invokeMethod(app, [enabled]() {
            voice_ui_state.vad_enabled = enabled;
        }, Qt::QueuedConnection);
    }
}

inline void voice_set_vad_threshold(int threshold_db, bool persist = true) {
    voice_client.vad_threshold_db = qBound(-80, threshold_db, -10);
    {
        QWriteLocker locker(&config.lock);
        config.vad_threshold_db = voice_client.vad_threshold_db;
    }
    if (persist) {
        write_config_async();
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        QMetaObject::invokeMethod(app, [thresh = voice_client.vad_threshold_db]() {
            voice_ui_state.vad_threshold_db = thresh;
        }, Qt::QueuedConnection);
    }
}

inline QAudioFormat voice_audio_format() {
    QAudioFormat format;
    format.setSampleRate(VOICE_AUDIO_SAMPLE_RATE);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

inline QAudioDevice voice_input_device() {
    if (voice_client.input_device_id.isEmpty()) {
        return QMediaDevices::defaultAudioInput();
    }

    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        if (device.id() == voice_client.input_device_id) return device;
    }

    qWarning() << "Selected microphone is unavailable; using the default input device.";
    voice_client.input_device_id.clear();
    return QMediaDevices::defaultAudioInput();
}

inline QAudioDevice voice_output_device() {
    if (voice_client.output_device_id.isEmpty()) {
        return QMediaDevices::defaultAudioOutput();
    }

    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        if (device.id() == voice_client.output_device_id) return device;
    }

    qWarning() << "Selected playback device is unavailable; using the default output device.";
    voice_client.output_device_id.clear();
    return QMediaDevices::defaultAudioOutput();
}

inline void voice_start_audio_input() {
    if (voice_client.audio_source) {
        voice_client.audio_source->stop();
        delete voice_client.audio_source;
        voice_client.audio_source = nullptr;
        voice_client.input_stream = nullptr;
    }

    QAudioDevice device = voice_input_device();
    QAudioFormat format = voice_audio_format();
    if (device.isNull() || !device.isFormatSupported(format)) {
        // qDebug() << "Selected microphone does not support 48kHz mono format.";
        return;
    }

    voice_codec_initialize(voice_client.bitrate_kbps);

    voice_client.audio_source = new QAudioSource(device, format);
    voice_client.audio_source->setBufferSize(VOICE_AUDIO_BUFFER_SIZE);
    voice_client.input_stream = voice_client.audio_source->start();
    if (!voice_client.input_stream) {
        qWarning() << "Could not start microphone:" << voice_client.audio_source->error();
        return;
    }

    // qDebug() << "Voice microphone started:" << device.description()
    //          << "format:" << format.sampleRate() << "Hz, 16-bit mono";
    QObject::connect(voice_client.audio_source, &QAudioSource::stateChanged, [](QAudio::State state) {
        if (voice_client.audio_source && voice_client.audio_source->error() != QAudio::NoError) {
            qWarning() << "Microphone error, state:" << state << "error:" << voice_client.audio_source->error();
        }
    });
    QObject::connect(voice_client.input_stream, &QIODevice::readyRead, []() {
        if (!voice_client.input_stream) return;
        QByteArray captured_audio = voice_client.input_stream->readAll();
        voice_client.pending_pcm.append(captured_audio);
        if (voice_client.is_mute) {
            voice_client.pending_pcm.clear();
            voice_update_local_speaker(false, QDateTime::currentMSecsSinceEpoch());
            return;
        }

        while (voice_client.pending_pcm.size() >= VOICE_AUDIO_FRAME_SIZE && !voice_client.is_mute) {
            QByteArray pcm_frame = voice_client.pending_pcm.left(VOICE_AUDIO_FRAME_SIZE);
            voice_client.pending_pcm.remove(0, VOICE_AUDIO_FRAME_SIZE);
            if (!voice_client.encoder) continue;

            opus_int16 pcm[VOICE_AUDIO_FRAME_SAMPLES];
            double sum_sq = 0.0;
            double gain = voice_client.input_volume / 100.0;
            for (int i = 0; i < VOICE_AUDIO_FRAME_SAMPLES; ++i) {
                qint16 val = qFromLittleEndian<qint16>(
                    reinterpret_cast<const uchar *>(pcm_frame.constData() + i * 2));
                if (gain != 1.0) {
                    val = static_cast<qint16>(qBound(-32768, static_cast<int>(val * gain), 32767));
                }
                pcm[i] = val;
                sum_sq += (double)val * val;
            }

            double rms = std::sqrt(sum_sq / (double)VOICE_AUDIO_FRAME_SAMPLES);
            double db = (rms > 1.0) ? 20.0 * std::log10(rms / 32768.0) : -100.0;

            if (voice_mic_test_active) {
                static qint64 last_test_emit_ms = 0;
                qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
                if (now_ms - last_test_emit_ms >= 80) {
                    last_test_emit_ms = now_ms;
                    int level = qBound(0, static_cast<int>((db + 70.0) / 70.0 * 100.0), 100);
                    QCoreApplication *app = QCoreApplication::instance();
                    if (app && on_voice_mic_test_level) {
                        QMetaObject::invokeMethod(app, [level, db]() {
                            if (on_voice_mic_test_level) on_voice_mic_test_level(level, db);
                        }, Qt::QueuedConnection);
                    }
                }

                if (voice_client.output_stream && !voice_client.is_deaf) {
                    QByteArray loopback_pcm(VOICE_AUDIO_FRAME_SIZE, Qt::Uninitialized);
                    for (int k = 0; k < VOICE_AUDIO_FRAME_SAMPLES; ++k) {
                        qToLittleEndian<qint16>(pcm[k], reinterpret_cast<uchar *>(loopback_pcm.data() + k * 2));
                    }
                    voice_client.output_stream->write(loopback_pcm);
                }
                continue;
            }

            if (voice_client.current_room_id == 0) {
                continue;
            }

            if (voice_client.vad_enabled) {
                if (db < voice_client.vad_threshold_db) {
                    voice_update_local_speaker(false, QDateTime::currentMSecsSinceEpoch());
                    continue;
                }
            }

            voice_update_local_speaker(true, QDateTime::currentMSecsSinceEpoch());

            unsigned char encoded[VOICE_MAX_AUDIO_PAYLOAD];
            int encoded_size = opus_encode(
                voice_client.encoder, pcm, VOICE_AUDIO_FRAME_SAMPLES,
                encoded, VOICE_MAX_AUDIO_PAYLOAD);
            if (encoded_size < 0) {
                qWarning() << "Opus encoding failed:" << opus_strerror(encoded_size);
                continue;
            }

            voice_send_packet(VOICE_DATA, QByteArray(
                reinterpret_cast<const char *>(encoded), encoded_size));
        }
    });
}

inline void voice_start_audio_output() {
    if (voice_client.audio_sink) {
        voice_client.audio_sink->stop();
        delete voice_client.audio_sink;
        voice_client.audio_sink = nullptr;
        voice_client.output_stream = nullptr;
    }

    QAudioDevice device = voice_output_device();
    QAudioFormat format = voice_audio_format();
    if (device.isNull() || !device.isFormatSupported(format)) {
        // qDebug() << "Selected playback device does not support 48kHz mono format.";
        return;
    }

    voice_client.audio_sink = new QAudioSink(device, format);
    voice_client.audio_sink->setBufferSize(VOICE_AUDIO_BUFFER_SIZE);
    voice_client.audio_sink->setVolume(qBound(0.0, voice_client.output_volume / 100.0, 1.5));
    voice_client.output_stream = voice_client.audio_sink->start();
    if (!voice_client.output_stream) {
        qWarning() << "Could not start playback device:" << device.description()
                   << "error:" << voice_client.audio_sink->error();
        return;
    }

    // qDebug() << "Voice playback started:" << device.description();
    QObject::connect(voice_client.audio_sink, &QAudioSink::stateChanged, [](QAudio::State state) {
        if (voice_client.audio_sink && voice_client.audio_sink->error() != QAudio::NoError) {
            qWarning() << "Playback error, state:" << state << "error:" << voice_client.audio_sink->error();
        }
    });
}

inline void voice_play_audio_frame() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    voice_update_local_speaker(false, now);
    if (voice_client.is_deaf || voice_mic_test_active || !voice_client.audio_sink) return;

    if (voice_client.audio_sink->state() == QAudio::SuspendedState) {
        voice_client.audio_sink->resume();
    } else if (voice_client.audio_sink->state() == QAudio::StoppedState || !voice_client.output_stream || !voice_client.output_stream->isOpen()) {
        voice_client.output_stream = voice_client.audio_sink->start();
    }
    if (!voice_client.output_stream) return;

    qint32 mixed_samples[VOICE_AUDIO_FRAME_SIZE / 2] = {};
    bool has_audio = false;
    int active_peers = 0;
    bool speaker_set_changed = false;

    for (auto it = voice_client.incoming_audio.begin(); it != voice_client.incoming_audio.end(); ) {
        if (now - it.value().last_packet_at > 200) {
            if (it.value().decoder) opus_decoder_destroy(it.value().decoder);
            if (voice_client.active_speakers.remove(it.key())) speaker_set_changed = true;
            it = voice_client.incoming_audio.erase(it);
            continue;
        }

        auto &peer = it.value();
        if (!peer.decoder) {
            ++it;
            continue;
        }

        if (!peer.playback_started && peer.encoded_packets.size() < 3) {
            ++it;
            continue;
        }
        peer.playback_started = true;

        if (peer.pcm.size() < VOICE_AUDIO_FRAME_SIZE) {
            opus_int16 decoded[VOICE_AUDIO_FRAME_SAMPLES];
            int decoded_samples;
            if (!peer.encoded_packets.isEmpty()) {
                QByteArray encoded = peer.encoded_packets.dequeue();
                decoded_samples = opus_decode(
                    peer.decoder,
                    reinterpret_cast<const unsigned char *>(encoded.constData()),
                    encoded.size(), decoded, VOICE_AUDIO_FRAME_SAMPLES, 0);
            } else {
                decoded_samples = opus_decode(
                    peer.decoder, nullptr, 0, decoded, VOICE_AUDIO_FRAME_SAMPLES, 0);
            }

            if (decoded_samples < 0) {
                qWarning() << "Opus decoding failed:" << opus_strerror(decoded_samples);
                ++it;
                continue;
            }

            QByteArray decoded_pcm(decoded_samples * 2, Qt::Uninitialized);
            for (int i = 0; i < decoded_samples; ++i) {
                qToLittleEndian<qint16>(
                    decoded[i], reinterpret_cast<uchar *>(decoded_pcm.data() + i * 2));
            }
            peer.pcm.append(decoded_pcm);
        }

        if (peer.pcm.size() >= VOICE_AUDIO_FRAME_SIZE) {
            double u_gain = voice_get_user_volume(it.key()) / 100.0;
            for (int i = 0; i < VOICE_AUDIO_FRAME_SAMPLES; ++i) {
                qint16 sample = qFromLittleEndian<qint16>(
                    reinterpret_cast<const uchar *>(peer.pcm.constData() + i * 2));
                if (u_gain != 1.0) {
                    sample = static_cast<qint16>(qBound(-32768, static_cast<int>(sample * u_gain), 32767));
                }
                mixed_samples[i] += sample;
            }
            peer.pcm.remove(0, VOICE_AUDIO_FRAME_SIZE);
            has_audio = true;
            ++active_peers;
        }
        ++it;
    }

    for (auto it = voice_client.incoming_audio.begin(); it != voice_client.incoming_audio.end(); ++it) {
        if (voice_client.active_speakers.contains(it.key()) &&
            now - it.value().last_voice_activity_at > 250) {
            voice_client.active_speakers.remove(it.key());
            speaker_set_changed = true;
        }
    }
    if (speaker_set_changed) voice_emit_active_speakers(voice_client.active_speakers);

    if (!has_audio) return;

    QByteArray mixed_pcm(VOICE_AUDIO_FRAME_SIZE, Qt::Uninitialized);
    for (int i = 0; i < VOICE_AUDIO_FRAME_SIZE / 2; ++i) {
        qint32 averaged_sample = mixed_samples[i] / active_peers;
        qint16 sample = static_cast<qint16>(qBound(-32768, averaged_sample, 32767));
        qToLittleEndian<qint16>(
            sample, reinterpret_cast<uchar *>(mixed_pcm.data() + i * 2));
    }

    qint64 written = voice_client.output_stream->write(mixed_pcm);
    if (written != mixed_pcm.size()) {
        qWarning() << "Playback accepted" << written << "of" << mixed_pcm.size() << "mixed audio bytes";
    }
}

inline void voice_set_input_device_impl(const QByteArray &device_id) {
    if (voice_client.input_device_id == device_id) return;
    voice_client.input_device_id = device_id;
    voice_client.pending_pcm.clear();
    if (voice_client.current_room_id > 0) voice_start_audio_input();
}

inline void voice_set_output_device_impl(const QByteArray &device_id) {
    if (voice_client.output_device_id == device_id) return;
    voice_client.output_device_id = device_id;
    if (voice_client.current_room_id > 0) voice_start_audio_output();
}

inline void voice_clear_incoming_audio() {
    for (auto it = voice_client.incoming_audio.begin(); it != voice_client.incoming_audio.end(); ++it) {
        if (it.value().decoder) opus_decoder_destroy(it.value().decoder);
    }
    voice_client.incoming_audio.clear();
    QSet<uint64_t> previous_speakers = voice_client.active_speakers;
    voice_client.active_speakers.clear();
    if (voice_client.current_room_id > 0 && !voice_client.is_mute &&
        voice_client.last_local_voice_activity_at > 0 &&
        QDateTime::currentMSecsSinceEpoch() - voice_client.last_local_voice_activity_at <= 250) {
        voice_client.active_speakers.insert(voice_client.user_id);
    }
    if (voice_client.active_speakers != previous_speakers) {
        voice_emit_active_speakers(voice_client.active_speakers);
    }
}

inline void voice_handle_datagram(const QByteArray &data, uint64_t generation) {
    if (generation != voice_client.start_generation ||
        data.size() < static_cast<int>(sizeof(voice_packet_header_t))) {
        return;
    }

    voice_packet_header_t header;
    std::memcpy(&header, data.constData(), sizeof(header));
    if (header.magic != 0x4543484F ||
        header.room_id != voice_client.current_room_id ||
        header.payload_size != data.size() -
            static_cast<int>(sizeof(voice_packet_header_t)) ||
        header.packet_type != VOICE_DATA || voice_client.is_deaf) {
        return;
    }

    QByteArray encoded_audio = data.mid(sizeof(voice_packet_header_t));
    if (encoded_audio.isEmpty() || encoded_audio.size() > VOICE_MAX_AUDIO_PAYLOAD) {
        return;
    }

    static uint64_t received_audio_packets = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto &peer = voice_client.incoming_audio[header.user_id];
    if (!peer.decoder) {
        int opus_error = OPUS_OK;
        peer.decoder = opus_decoder_create(
            VOICE_AUDIO_SAMPLE_RATE, 1, &opus_error);
        if (!peer.decoder || opus_error != OPUS_OK) {
            qWarning() << "Could not create Opus decoder:"
                       << opus_strerror(opus_error);
            peer.decoder = nullptr;
            voice_client.incoming_audio.remove(header.user_id);
            return;
        }
    }

    peer.last_voice_activity_at = now;
    if (!voice_client.active_speakers.contains(header.user_id)) {
        voice_client.active_speakers.insert(header.user_id);
        voice_emit_active_speakers(voice_client.active_speakers);
    }
    peer.encoded_packets.enqueue(encoded_audio);
    peer.last_packet_at = now;

    constexpr int max_queued_packets = 25;
    while (peer.encoded_packets.size() > max_queued_packets) {
        peer.encoded_packets.dequeue();
    }

    // if (++received_audio_packets == 1 || received_audio_packets % 100 == 0) {
    //     qDebug() << "Received Opus audio packets:" << received_audio_packets;
    // }
}

inline void voice_send_packet(uint8_t packet_type, const QByteArray &payload) {
    if (!voice_client.udp_ready || voice_client.current_room_id == 0) return;
    if (payload.size() > VOICE_MAX_AUDIO_PAYLOAD) {
        qWarning() << "Rejected invalid voice audio payload size:" << payload.size();
        return;
    }

    voice_packet_header_t hdr;
    hdr.magic = 0x4543484F;
    hdr.packet_type = packet_type;
    hdr.user_id = voice_client.user_id;
    hdr.room_id = voice_client.current_room_id;
    hdr.seq = ++voice_client.tx_seq;
    hdr.is_mute = voice_client.is_mute ? 1 : 0;
    hdr.is_deaf = voice_client.is_deaf ? 1 : 0;
    hdr.payload_size = (uint16_t)payload.size();

    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&hdr), sizeof(voice_packet_header_t));
    if (!payload.isEmpty()) {
        packet.append(payload);
    }

    voice_udp_send_packet(packet, voice_client.server_address,
                          voice_client.server_port);
    if (packet_type == VOICE_DATA) {
        // static uint64_t sent_audio_packets = 0;
        // if (++sent_audio_packets == 1 || sent_audio_packets % 100 == 0) {
        //     qDebug() << "Sent voice audio packets:" << sent_audio_packets;
        // }
    }
}

inline void voice_stop_channel_impl(bool notify_server) {
    if (voice_client.current_room_id == 0) return;

    ++voice_client.start_generation;
    uint64_t old_rid = voice_client.current_room_id;
    if (notify_server) {
        voice_send_packet(VOICE_LEAVE);
        voice_request_leave_room(old_rid);
    }

    if (voice_client.ping_timer) {
        voice_client.ping_timer->stop();
        delete voice_client.ping_timer;
        voice_client.ping_timer = nullptr;
    }

    if (voice_client.audio_source) {
        voice_client.audio_source->stop();
        delete voice_client.audio_source;
        voice_client.audio_source = nullptr;
        voice_client.input_stream = nullptr;
    }

    if (voice_client.audio_sink) {
        voice_client.audio_sink->stop();
        delete voice_client.audio_sink;
        voice_client.audio_sink = nullptr;
        voice_client.output_stream = nullptr;
    }

    if (voice_client.playback_timer) {
        voice_client.playback_timer->stop();
        delete voice_client.playback_timer;
        voice_client.playback_timer = nullptr;
    }

    voice_udp_close();
    voice_client.udp_ready = false;

    voice_client.current_room_id = 0;
    voice_client.current_room_name.clear();
    voice_client.tx_seq = 0;
    voice_client.pending_pcm.clear();
    voice_client.last_local_voice_activity_at = 0;
    voice_clear_incoming_audio();
    if (voice_client.encoder) {
        opus_encoder_destroy(voice_client.encoder);
        voice_client.encoder = nullptr;
    }

    sound_play_voice_leave();
    // qDebug() << "Voice channel closed for room:" << old_rid;
    voice_emit_state_changed(false, 0, "");
}

inline void voice_finish_start_channel(
    uint64_t room_id, const QString &room_name, const QString &host,
    quint16 port, const QHostAddress &server_address, uint64_t start_generation) {
    if (voice_client.start_generation != start_generation ||
        voice_client.current_room_id != room_id) {
        return;
    }

    voice_client.server_host = host;
    voice_client.server_address = server_address;
    voice_client.server_port = port;

    QString udp_error;
    voice_client.udp_ready = voice_udp_open(start_generation, udp_error);
    if (!voice_client.udp_ready) {
        voice_fail_start_channel(
            room_id, start_generation,
            "Could not open voice UDP socket: " + udp_error);
        return;
    }

    voice_start_audio_input();
    voice_start_audio_output();
    voice_clear_incoming_audio();
    voice_client.playback_timer = new QTimer();
    QObject::connect(voice_client.playback_timer, &QTimer::timeout, voice_play_audio_frame);
    voice_client.playback_timer->start(10);

    voice_client.ping_timer = new QTimer();
    QObject::connect(voice_client.ping_timer, &QTimer::timeout, []() {
        voice_send_packet(VOICE_PING);
    });
    voice_client.ping_timer->start(2000);

    voice_send_packet(VOICE_STATE);
    voice_send_packet(VOICE_PING);

    // qDebug() << "Voice channel connected to room:" << room_id << room_name;
    voice_emit_state_changed(true, room_id, room_name);
    sound_play_voice_join();
}

inline void voice_fail_start_channel(
    uint64_t room_id, uint64_t start_generation, const QString &error) {
    if (voice_client.start_generation != start_generation ||
        voice_client.current_room_id != room_id) {
        return;
    }

    qWarning() << error;
    voice_request_leave_room(room_id);
    voice_client.current_room_id = 0;
    voice_client.current_room_name.clear();
    voice_client.last_local_voice_activity_at = 0;
    voice_clear_incoming_audio();
    voice_emit_state_changed(false, 0, "");
}

inline void voice_start_channel_impl(
    uint64_t room_id, const QString &room_name, const QString &server_entry,
    uint64_t user_id) {
    if (voice_client.current_room_id == room_id) {
        // Clicking active voice room toggles disconnect
        voice_stop_channel_impl(true);
        return;
    }

    if (voice_client.current_room_id > 0) {
        voice_stop_channel_impl(true);
    }

    uint64_t start_generation = ++voice_client.start_generation;
    voice_client.current_room_id = room_id;
    voice_client.user_id = user_id;
    voice_client.current_room_name = room_name;

    QString parsed_server_entry = server_entry.trimmed();
    QString host;
    quint16 port = 5555;

    if (parsed_server_entry.startsWith("[")) {
        int closing_bracket = parsed_server_entry.indexOf(']');
        if (closing_bracket < 0) {
            voice_fail_start_channel(
                room_id, start_generation,
                "Invalid voice server address: " + parsed_server_entry);
            return;
        }

        host = parsed_server_entry.mid(1, closing_bracket - 1);
        QString port_text = parsed_server_entry.mid(closing_bracket + 1);
        if (!port_text.isEmpty()) {
            bool port_ok = false;
            uint parsed_port = port_text.startsWith(':')
                                   ? port_text.mid(1).toUInt(&port_ok)
                                   : 0;
            if (!port_ok || parsed_port == 0 || parsed_port > 65535) {
                voice_fail_start_channel(
                    room_id, start_generation,
                    "Invalid voice server port: " + parsed_server_entry);
                return;
            }
            port = static_cast<quint16>(parsed_port);
        }
    } else {
        int colon_index = parsed_server_entry.lastIndexOf(':');
        if (colon_index > 0 && parsed_server_entry.indexOf(':') == colon_index) {
            bool port_ok = false;
            uint parsed_port = parsed_server_entry.mid(colon_index + 1).toUInt(&port_ok);
            if (port_ok && parsed_port > 0 && parsed_port <= 65535) {
                host = parsed_server_entry.left(colon_index);
                port = static_cast<quint16>(parsed_port);
            } else {
                host = parsed_server_entry;
            }
        } else {
            host = parsed_server_entry;
        }
    }

    if (host.isEmpty()) {
        voice_fail_start_channel(
            room_id, start_generation, "Voice server address is empty");
        return;
    }

    QHostAddress server_address(host);
    if (!server_address.isNull()) {
        voice_finish_start_channel(
            room_id, room_name, host, port, server_address, start_generation);
        return;
    }

    QHostInfo::lookupHost(
        host, voice_worker().context,
        [room_id, room_name, host, port, start_generation](const QHostInfo &host_info) {
            if (voice_client.start_generation != start_generation ||
                voice_client.current_room_id != room_id) {
                return;
            }

            QHostAddress resolved_address;
            for (const QHostAddress &address : host_info.addresses()) {
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    resolved_address = address;
                    break;
                }
                if (resolved_address.isNull()) resolved_address = address;
            }

            if (resolved_address.isNull()) {
                voice_fail_start_channel(
                    room_id, start_generation,
                    "Could not resolve voice server host: " + host);
                return;
            }

            voice_finish_start_channel(
                room_id, room_name, host, port, resolved_address, start_generation);
        });
}

inline void voice_toggle_mute_impl() {
    voice_client.is_mute = !voice_client.is_mute;
    if (voice_client.is_mute) {
        voice_update_local_speaker(false, QDateTime::currentMSecsSinceEpoch());
    }
    voice_send_packet(VOICE_STATE);
    voice_emit_state_changed(
        voice_client.current_room_id > 0,
        voice_client.current_room_id,
        voice_client.current_room_name);
}

inline void voice_toggle_deaf_impl() {
    voice_client.is_deaf = !voice_client.is_deaf;
    if (voice_client.is_deaf) voice_clear_incoming_audio();
    voice_send_packet(VOICE_STATE);
    voice_emit_state_changed(
        voice_client.current_room_id > 0,
        voice_client.current_room_id,
        voice_client.current_room_name);
}

inline void start_voice_channel(uint64_t room_id, const QString &room_name) {
    QString server_entry = client.server.address;
    uint64_t user_id = client.user_id;
    voice_post([room_id, room_name, server_entry, user_id]() {
        voice_start_channel_impl(room_id, room_name, server_entry, user_id);
    });
}

inline void stop_voice_channel(bool notify_server = true) {
    voice_post([notify_server]() { voice_stop_channel_impl(notify_server); });
}

inline void toggle_voice_mute() {
    voice_post([]() { voice_toggle_mute_impl(); });
}

inline void toggle_voice_deaf() {
    voice_post([]() { voice_toggle_deaf_impl(); });
}

inline void voice_set_bitrate(int bitrate_kbps, bool persist = true) {
    int bounded_bitrate = qBound(16, bitrate_kbps, 128);
    voice_ui_state.bitrate_kbps = bounded_bitrate;
    voice_post([bounded_bitrate, persist]() {
        voice_set_bitrate_impl(bounded_bitrate, persist);
    });
}

inline void voice_set_input_device(const QByteArray &device_id) {
    voice_ui_state.input_device_id = device_id;
    voice_post([device_id]() { voice_set_input_device_impl(device_id); });
}

inline void voice_set_output_device(const QByteArray &device_id) {
    voice_ui_state.output_device_id = device_id;
    voice_post([device_id]() { voice_set_output_device_impl(device_id); });
}

inline void voice_worker_shutdown(bool wait_for_stop = false) {
    voice_worker_t &worker = voice_worker();
    bool queued = QMetaObject::invokeMethod(
        worker.context, []() {
            voice_stop_channel_impl(false);
            voice_udp_shutdown();
            QThread::currentThread()->quit();
        }, Qt::QueuedConnection);
    if (!queued) {
        qWarning() << "Could not queue voice worker shutdown.";
        worker.thread.quit();
    }
    if (wait_for_stop) worker.thread.wait();
}

inline void voice_start_mic_test() {
    voice_post([]() {
        voice_mic_test_active = true;
        if (!voice_client.audio_source) {
            voice_start_audio_input();
        }
        if (!voice_client.audio_sink) {
            voice_start_audio_output();
        }
    });
}

inline void voice_stop_mic_test() {
    voice_post([]() {
        voice_mic_test_active = false;
        if (voice_client.current_room_id == 0) {
            if (voice_client.audio_source) {
                voice_client.audio_source->stop();
                delete voice_client.audio_source;
                voice_client.audio_source = nullptr;
                voice_client.input_stream = nullptr;
            }
            if (voice_client.audio_sink) {
                voice_client.audio_sink->stop();
                delete voice_client.audio_sink;
                voice_client.audio_sink = nullptr;
                voice_client.output_stream = nullptr;
            }
        }
    });
}

#endif // CLIENT_VOICE_H
