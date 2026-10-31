#ifndef CLIENT_FILE_H
#define CLIENT_FILE_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QPixmap>
#include <QImage>
#include <QThreadPool>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QClipboard>
#include <QApplication>
#include <QRegularExpression>
#include <functional>
#include "utils/asn1_parser.h"
#include "common/types.h"

inline std::function<void(uint64_t room_id, const QString &file_path, const QString &local_abs_path)> on_file_download_completed = nullptr;
inline std::function<void(const QString &err)> on_file_error = nullptr;
inline std::function<void(const QString &avatar_path)> on_runtime_avatar_ready = nullptr;

inline QMap<QString, QByteArray> runtime_avatar_buffers;
inline QMap<QString, QPixmap> runtime_avatar_pixmaps;
inline QMap<QString, QPixmap> room_avatar_crop_cache;
inline QSet<QString> pending_avatar_downloads;

inline QMap<QString, QByteArray> runtime_chat_image_buffers;
inline QMap<QString, QByteArray> runtime_chat_image_data;
inline QSet<QString> pending_chat_image_downloads;

inline void clear_pending_chat_downloads() {
    pending_chat_image_downloads.clear();
}

inline QString get_temp_preview_dir() {
    QString p = QDir::tempPath() + "/echo_preview";
    QDir().mkpath(p);
    return p;
}

inline void clear_temporary_image_cache() {
    QString preview_path = QDir::tempPath() + "/echo_preview";
    QDir dir(preview_path);
    if (dir.exists()) {
        dir.removeRecursively();
        dir.mkpath(".");
    }
    QDir temp_root(QDir::tempPath());
    QStringList legacy = temp_root.entryList(QStringList() << "echo_*", QDir::Files);
    for (const QString &f : legacy) {
        temp_root.remove(f);
    }
}

inline void open_local_file_detached(const QString &file_path) {
    if (file_path.isEmpty()) return;
    QThreadPool::globalInstance()->start([file_path]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(file_path));
    });
}

inline void show_in_file_manager(const QString &file_path) {
    if (file_path.isEmpty()) return;
    QThreadPool::globalInstance()->start([file_path]() {
#ifdef Q_OS_WIN
        QString native_path = QDir::toNativeSeparators(file_path);
        QFileInfo fi(native_path);
        if (fi.exists()) {
            QString cmd = QString("explorer.exe /select,\"%1\"").arg(native_path);
            QProcess::startDetached(cmd);
        } else if (fi.dir().exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
        }
#elif defined(Q_OS_MAC)
        QStringList args;
        args << "-e" << "tell application \"Finder\""
             << "-e" << "activate"
             << "-e" << "select POSIX file \"" + file_path + "\""
             << "-e" << "end tell";
        QProcess::startDetached("osascript", args);
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file_path).absolutePath()));
#endif
    });
}

inline void copy_image_to_clipboard(const QByteArray &data, const QString &file_path = "") {
    QImage img;
    if (!data.isEmpty()) {
        img.loadFromData(data);
    } else if (!file_path.isEmpty() && QFile::exists(file_path)) {
        img.load(file_path);
    }
    if (!img.isNull()) {
        QApplication::clipboard()->setImage(img);
    }
}

inline bool is_valid_avatar_mime(const QByteArray &data, const QString &filename) {
    QString ext = QFileInfo(filename).suffix().toLower();
    static const QSet<QString> allowed_exts = {"png", "jpg", "jpeg", "webp", "avif", "bmp", "gif"};
    if (!allowed_exts.contains(ext)) return false;

    if (data.size() < 4) return false;
    const unsigned char *b = reinterpret_cast<const unsigned char*>(data.constData());

    if (ext == "png" && data.size() >= 8) {
        return (b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G' &&
                b[4] == 0x0D && b[5] == 0x0A && b[6] == 0x1A && b[7] == 0x0A);
    }
    if ((ext == "jpg" || ext == "jpeg") && data.size() >= 3) {
        return (b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF);
    }
    if (ext == "gif" && data.size() >= 6) {
        return (data.startsWith("GIF87a") || data.startsWith("GIF89a"));
    }
    if (ext == "bmp" && data.size() >= 2) {
        return (b[0] == 0x42 && b[1] == 0x4D);
    }
    if (ext == "webp" && data.size() >= 12) {
        return (data.startsWith("RIFF") && data.mid(8, 4) == "WEBP");
    }
    if (ext == "avif" && data.size() >= 12) {
        return (data.mid(4, 4) == "ftyp" &&
               (data.mid(8, 4) == "avif" || data.mid(8, 4) == "avis" || data.mid(8, 4) == "mif1"));
    }
    return true;
}

inline uint16_t detect_file_msg_type(const QString &file_path) {
    QString ext = QFileInfo(file_path).suffix().toLower();
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "webp" || ext == "bmp" || ext == "svg") {
        return MESSAGE_TYPE::IMAGE;
    }
    if (ext == "mp4" || ext == "webm" || ext == "mkv" || ext == "avi" || ext == "mov") {
        return MESSAGE_TYPE::VIDEO;
    }
    return MESSAGE_TYPE::OTHER;
}

inline QString get_server_cache_dir() {
    QString srv = client.server.address;
    if (srv.isEmpty()) srv = "default";
    srv.replace(":", "_").replace("/", "_").replace("\\", "_");
    return QCoreApplication::applicationDirPath() + "/cache/" + srv;
}

inline QString get_cached_file_path(const QString &rel_path) {
    if (rel_path.isEmpty()) return "";
    QString clean = rel_path;
    clean.replace("\\", "/");
    QString cache_abs = get_server_cache_dir() + "/" + clean;
    if (QFile::exists(cache_abs)) {
        return QDir::toNativeSeparators(cache_abs);
    }
    return "";
}

inline void user_request_file_download(uint64_t room_id, const QString &rel_path, uint64_t offset = 0) {
    if (!asn1_client_is_connected() || rel_path.isEmpty()) return;

    QString clean = rel_path;
    clean.replace("\\", "/");

    bool is_avatar = clean.startsWith("files/avt/");
    if (!is_avatar && room_id == 0) return;

    if (is_avatar && offset == 0) {
        if (runtime_avatar_pixmaps.contains(clean) || pending_avatar_downloads.contains(clean)) return;
        pending_avatar_downloads.insert(clean);
    } else if (detect_file_msg_type(clean) == MESSAGE_TYPE::IMAGE && offset == 0) {
        if (runtime_chat_image_data.contains(clean) || pending_chat_image_downloads.contains(clean)) return;
        pending_chat_image_downloads.insert(clean);
    }

    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;

    asn1_write_value(element, "", "file-download-req", 1);
    asn1_write_unsigned64(element, "file-download-req.room-id", room_id);
    asn1_write_string(element, "file-download-req.file-path", clean);
    asn1_write_unsigned64(element, "file-download-req.offset", offset);

    asn1_client_send_packet(element);
}

inline void receive_file_download_chunk(asn1_node element) {
    uint64_t room_id = asn1_read_int(element, "file-download-chunk.room-id");
    QString rel_path = asn1_read_string(element, "file-download-chunk.file-path");
    rel_path.replace("\\", "/");
    uint64_t total_size = asn1_read_int(element, "file-download-chunk.total-size");
    uint64_t offset = asn1_read_int(element, "file-download-chunk.offset");
    QByteArray data = asn1_read_bytes(element, "file-download-chunk.data");

    if (rel_path.isEmpty() || data.isEmpty()) return;

    // Avatars are loaded strictly in memory at runtime without disk caching
    if (rel_path.startsWith("files/avt/")) {
        if (offset == 0) {
            runtime_avatar_buffers[rel_path].clear();
        }
        runtime_avatar_buffers[rel_path].append(data);

        uint64_t next_offset = offset + data.size();
        if (next_offset < total_size) {
            asn1_node req_elem;
            if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &req_elem) == ASN1_SUCCESS) {
                asn1_write_value(req_elem, "", "file-download-req", 1);
                asn1_write_unsigned64(req_elem, "file-download-req.room-id", room_id);
                asn1_write_string(req_elem, "file-download-req.file-path", rel_path);
                asn1_write_unsigned64(req_elem, "file-download-req.offset", next_offset);
                asn1_client_send_packet(req_elem);
                asn1_delete_structure(&req_elem);
            }
        } else {
            pending_avatar_downloads.remove(rel_path);
            // qDebug() << "Runtime avatar download complete:" << rel_path;
            QPixmap pix;
            if (pix.loadFromData(runtime_avatar_buffers[rel_path])) {
                if (pix.width() > 512 || pix.height() > 512) {
                    pix = pix.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                runtime_avatar_pixmaps[rel_path] = pix;
            } else {
                qWarning() << "Failed to parse downloaded avatar pixmap:" << rel_path;
            }
            runtime_avatar_buffers.remove(rel_path);

            if (on_runtime_avatar_ready) {
                on_runtime_avatar_ready(rel_path);
            }
        }
        return;
    }

    // Chat images are loaded strictly in memory at runtime without disk caching
    if (detect_file_msg_type(rel_path) == MESSAGE_TYPE::IMAGE) {
        if (offset == 0) {
            runtime_chat_image_buffers[rel_path].clear();
        }
        runtime_chat_image_buffers[rel_path].append(data);

        uint64_t next_offset = offset + data.size();
        if (next_offset < total_size) {
            asn1_node req_elem;
            if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &req_elem) == ASN1_SUCCESS) {
                asn1_write_value(req_elem, "", "file-download-req", 1);
                asn1_write_unsigned64(req_elem, "file-download-req.room-id", room_id);
                asn1_write_string(req_elem, "file-download-req.file-path", rel_path);
                asn1_write_unsigned64(req_elem, "file-download-req.offset", next_offset);
                asn1_client_send_packet(req_elem);
                asn1_delete_structure(&req_elem);
            }
        } else {
            pending_chat_image_downloads.remove(rel_path);
            runtime_chat_image_data[rel_path] = runtime_chat_image_buffers.take(rel_path);
            // qDebug() << "Runtime chat image download complete (in-memory):" << rel_path;
            if (on_file_download_completed) {
                on_file_download_completed(room_id, rel_path, "");
            }
        }
        return;
    }

    QString cache_abs = QDir::toNativeSeparators(get_server_cache_dir() + "/" + rel_path);
    QFileInfo fi(cache_abs);
    QDir().mkpath(fi.dir().absolutePath());

    QFile file(cache_abs);
    QIODevice::OpenMode mode = (offset == 0) ? (QIODevice::WriteOnly) : (QIODevice::WriteOnly | QIODevice::Append);
    if (!file.open(mode)) {
        // qDebug() << "Cannot open local cache file for writing:" << cache_abs;
        return;
    }
    file.seek(offset);
    file.write(data);
    file.close();

    uint64_t next_offset = offset + data.size();
    if (next_offset < total_size) {
        user_request_file_download(room_id, rel_path, next_offset);
    } else {
        // qDebug() << "File download complete:" << cache_abs;
        if (on_file_download_completed) {
            on_file_download_completed(room_id, rel_path, cache_abs);
        }
    }
}

inline bool user_upload_file(uint64_t room_id, const QString &local_filepath) {
    if (!asn1_client_is_connected() || room_id == 0) return false;

    QFile file(local_filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (on_file_error) on_file_error("Cannot open file: " + local_filepath);
        return false;
    }

    uint64_t total_size = file.size();
    if (total_size > 10 * 1024 * 1024) { // 10MB limit
        if (on_file_error) on_file_error("File exceeds 10MB limit");
        return false;
    }

    QString upload_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString file_name = QFileInfo(local_filepath).fileName();
    uint16_t msg_type = detect_file_msg_type(local_filepath);

    const qint64 CHUNK_SIZE = 32768; // 32KB chunk
    uint64_t offset = 0;

    while (offset < total_size) {
        file.seek(offset);
        QByteArray chunk = file.read(CHUNK_SIZE);
        if (chunk.isEmpty()) break;

        asn1_node element;
        int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
        if (ret != ASN1_SUCCESS) break;

        asn1_write_value(element, "", "file-upload-chunk", 1);
        asn1_write_string(element, "file-upload-chunk.upload-id", upload_id);
        asn1_write_unsigned64(element, "file-upload-chunk.room-id", room_id);
        asn1_write_string(element, "file-upload-chunk.file-name", file_name);
        asn1_write_unsigned64(element, "file-upload-chunk.total-size", total_size);
        asn1_write_unsigned64(element, "file-upload-chunk.offset", offset);
        asn1_write_unsigned64(element, "file-upload-chunk.msg-type", msg_type);
        asn1_write_byte_array(element, "file-upload-chunk.data", chunk);

        asn1_client_send_packet(element);
        offset += chunk.size();
    }

    file.close();
    return true;
}

inline void view_avatar_full(const QString &avatar_path, const QString &username = "") {
    QString title = username.isEmpty() ? "avatar" : username;
    QString safe_name = title;
    safe_name.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");

    if (avatar_path.isEmpty()) {
        QString def_path = QCoreApplication::applicationDirPath() + "/assets/defaults/user.png";
        if (QFile::exists(def_path)) {
            open_local_file_detached(def_path);
        }
        return;
    }

    QString temp_file = get_temp_preview_dir() + "/avatar_" + safe_name + "_" + QFileInfo(avatar_path).fileName();

    if (runtime_avatar_buffers.contains(avatar_path) && !runtime_avatar_buffers[avatar_path].isEmpty()) {
        QFile f(temp_file);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(runtime_avatar_buffers[avatar_path]);
            f.close();
            open_local_file_detached(temp_file);
            return;
        }
    }

    if (runtime_avatar_pixmaps.contains(avatar_path)) {
        QPixmap p = runtime_avatar_pixmaps.value(avatar_path);
        if (!p.isNull()) {
            p.save(temp_file, "PNG");
            open_local_file_detached(temp_file);
            return;
        }
    }

    user_request_file_download(0, avatar_path, 0);
}

#endif // CLIENT_FILE_H
