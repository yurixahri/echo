#ifndef USER_H
#define USER_H

#include "utils/asn1_parser.h"
#include "utils/config.h"
#include "common/types.h"
#include "functions/file.h"
#include "functions/voice.h"
#include <functional>
#include <QSslSocket>
#include <QUuid>
#include <QRunnable>
#include <QThreadPool>
#include <QMetaObject>
#include <QTimer>

inline std::function<void()> on_server_list_changed = nullptr;
inline std::function<void(const QString &address)> on_connection_start = nullptr;
inline std::function<void()> on_connection_finish = nullptr;
inline std::function<void(const QString &err)> on_connection_fail = nullptr;
inline std::function<void()> on_disconnected = nullptr;
inline std::function<void(bool is_admin)> on_admin_status_changed = nullptr;
inline std::function<void(const QString &msg)> on_operation_success = nullptr;
inline std::function<void(const QString &err)> on_operation_error = nullptr;
inline std::function<void(const QString &msg)> on_user_kicked = nullptr;
inline std::function<void(uint64_t room_id, const QString &reason)> on_chat_kicked = nullptr;
inline std::function<void(const QString &err)> on_server_password_required = nullptr;
inline std::function<void(const user_info_t &info)> on_my_info_received = nullptr;
inline std::function<void(const QString &avatar_path)> on_avatar_updated = nullptr;
inline std::function<void(const QString &server_name)> on_server_name_changed = nullptr;
inline std::function<void(const QList<member_item_t>&)> on_member_list_received = nullptr;
inline std::function<void(const user_info_t &info)> on_member_detail_received = nullptr;
inline std::function<void(const QString &sender_name, uint64_t sender_id, const QString &msg)> on_user_poked = nullptr;

inline uint64_t desired_voice_room_id = 0;
inline server_t last_connected_server;
inline bool is_manual_disconnect = false;
inline QTimer *reconnect_timer = nullptr;

inline void handle_connection_fail(const QString &err) {
    if (client.state == FAILED || client.state == SUCCESS) return;
    client.state = FAILED;
    if (on_connection_fail) {
        on_connection_fail(err);
    }
}

inline void connect_server(){
    QReadLocker lock(&config.lock);
    // qDebug() << "Connected. Sending client-check-id (uuid:" << client.server.uuid << ", name:" << config.username << ")";
    asn1_node element = NULL;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "client-check-id", 1);
    asn1_write_string(element, "client-check-id.uuid", client.server.uuid);
    asn1_write_string(element, "client-check-id.name", config.username);
    if (client.server.password.isEmpty()) {
        asn1_write_value(element, "client-check-id.password", NULL, 0);
    } else {
        asn1_write_string(element, "client-check-id.password", client.server.password);
    }
    asn1_client_send_packet(element);
}

inline void start_connection(server_t target_server){
    if (reconnect_timer) {
        reconnect_timer->stop();
    }
    if (voice_ui_state.current_room_id > 0) {
        desired_voice_room_id = voice_ui_state.current_room_id;
        stop_voice_channel(false);
    }
    last_connected_server = target_server;
    is_manual_disconnect = false;

    asn1_client_stop_network();
    uint64_t connection_generation = ++client.network_generation;

    client.state = WAITTING_INIT;
    client.server = target_server;
    client.user_id = 0;
    client.is_admin = false;
    if (on_admin_status_changed) {
        on_admin_status_changed(false);
    }

    if (on_connection_start) {
        on_connection_start(target_server.address);
    }

    QStringList args = target_server.address.split(":");
    uint16_t port = args.size() <= 1 ? 5555 : args[1].toUInt();

    QString host = args.value(0);
    client.network_thread = new QThread();
    client.network_worker = new ClientNetworkWorker(target_server.is_ssl);
    client.network_worker->moveToThread(client.network_thread);

    ClientNetworkWorker *network_worker = client.network_worker;
    QThread *network_thread = client.network_thread;
    QObject *application = QCoreApplication::instance();

    auto trigger_auto_reconnect = []() {
        if (!is_manual_disconnect && !last_connected_server.address.isEmpty()) {
            if (!reconnect_timer) {
                reconnect_timer = new QTimer();
                QObject::connect(reconnect_timer, &QTimer::timeout, []() {
                    if (!is_manual_disconnect && !last_connected_server.address.isEmpty() &&
                        client.state != SUCCESS && client.state != WAITTING_INIT && client.state != WAITTING_PROOF) {
                        // qDebug() << "Auto-reconnecting to server:" << last_connected_server.address;
                        start_connection(last_connected_server);
                    }
                });
            }
            if (!reconnect_timer->isActive()) {
                reconnect_timer->start(3000);
            }
        }
    };

    QObject::connect(network_worker, &ClientNetworkWorker::connected, application,
                     [connection_generation]() {
        if (connection_generation != client.network_generation.load()) return;
        client.network_connected = true;
        connect_server();
    });
    QObject::connect(network_worker, &ClientNetworkWorker::disconnected, application,
                     [connection_generation, trigger_auto_reconnect]() {
        if (connection_generation != client.network_generation.load()) return;
        client.network_connected = false;
        // qDebug() << "disconnected";
        bool was_connected = (client.state == SUCCESS);
        if (was_connected) {
            client.state = FAILED;
            if (voice_ui_state.current_room_id > 0) {
                desired_voice_room_id = voice_ui_state.current_room_id;
            }
            if (on_disconnected) on_disconnected();
        } else {
            handle_connection_fail("Disconnected from server");
        }
        trigger_auto_reconnect();
    });
    QObject::connect(network_worker, &ClientNetworkWorker::network_error, application,
                     [connection_generation, trigger_auto_reconnect](const QString &error) {
        if (connection_generation != client.network_generation.load()) return;
        qWarning() << "TCP connection error:" << error;
        bool was_connected = (client.state == SUCCESS);
        client.network_connected = false;
        if (was_connected) {
            client.state = FAILED;
            if (voice_ui_state.current_room_id > 0) {
                desired_voice_room_id = voice_ui_state.current_room_id;
            }
            if (on_disconnected) on_disconnected();
            if (on_operation_error) on_operation_error(error);
        } else {
            handle_connection_fail(error);
        }
        trigger_auto_reconnect();
    });
    QObject::connect(network_worker, &ClientNetworkWorker::packet_received, application,
                     [network_worker, connection_generation](const QByteArray &packet) {
        if (connection_generation == client.network_generation.load()) {
            asn1_client_process_packet(packet);
        }
        if (connection_generation != client.network_generation.load()) return;
        bool queued = QMetaObject::invokeMethod(
            network_worker, [network_worker]() { network_worker->acknowledge_packet(); },
            Qt::QueuedConnection);
        if (!queued && connection_generation == client.network_generation.load()) {
            qWarning() << "Could not acknowledge received TCP packet.";
        }
    });

    client.network_thread->start();
    bool queued = QMetaObject::invokeMethod(
        network_worker, [network_worker, host, port]() { network_worker->start(host, port); },
        Qt::QueuedConnection);
    if (!queued) {
        qWarning() << "Could not start TCP network worker.";
        asn1_client_stop_network();
        handle_connection_fail("Could not start network worker");
    }
}

inline bool check_difficulty(const QByteArray &hash, int difficulty){
    if (difficulty < 0 || difficulty > hash.size() * 8) return false;

    int full_bytes = difficulty / 8;
    int remaining_bits = difficulty % 8;

    for (int i = 0; i < full_bytes; i++) {
        if (static_cast<quint8>(hash[i]) != 0) return false;
    }

    if (remaining_bits > 0) {
        quint8 value = static_cast<quint8>(hash[full_bytes]);
        quint8 mask = static_cast<quint8>(0xFF << (8 - remaining_bits));

        if ((value & mask) != 0) return false;
    }

    return true;
}

inline void do_challenge_proof(asn1_node element){
    client.state = WAITTING_PROOF;
    QString uuid = asn1_read_string(element, "new-user-challenge.uuid");
    QByteArray challenge = asn1_read_bytes(element, "new-user-challenge.challenge");
    int difficulty = asn1_read_int(element, "new-user-challenge.difficulty");
    // qDebug() << "Solving challenge for new uuid:" << uuid << "difficulty:" << difficulty;

    if (difficulty < 0 || difficulty > 256 || !client.network_worker) {
        handle_connection_fail("Invalid server challenge");
        return;
    }

    uint64_t connection_generation = client.network_generation.load();
    QThreadPool::globalInstance()->start(QRunnable::create(
        [uuid, challenge, difficulty, connection_generation]() {
            uint64_t solution = 0;
            while (true) {
                if (connection_generation != client.network_generation.load()) return;
                QByteArray data = uuid.toUtf8() + challenge + QByteArray::number(solution);
                QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
                if (check_difficulty(hash, difficulty)) break;
                ++solution;
            }

            QObject *application = QCoreApplication::instance();
            if (!application) return;
            QMetaObject::invokeMethod(application,
                [uuid, solution, connection_generation]() {
                    if (connection_generation != client.network_generation.load() ||
                        !asn1_client_is_connected()) return;

                    client.server.uuid = uuid;
                    // qDebug() << "Challenge solved with solution:" << solution;
                    asn1_node response = NULL;
                    if (asn1_create_element(
                            client.definitions, "ChatProtocol.ProtocolMessage",
                            &response) != ASN1_SUCCESS) {
                        handle_connection_fail("Failed to create challenge response");
                        return;
                    }
                    asn1_write_value(response, "", "new-user-proof", 1);
                    asn1_write_string(response, "new-user-proof.uuid", uuid);
                    asn1_write_unsigned64(response, "new-user-proof.solution", solution);
                    asn1_client_send_packet(response);
                    asn1_delete_structure(&response);
                },
                Qt::QueuedConnection);
        }));
}

inline void receive_user_check_id(uint16_t status, uint64_t my_id = 0, const QString &my_avt = "", const QString &server_name = "", bool is_admin = false, bool can_kick = false){
    // qDebug() << "Received user_check_id response status:" << status << "User ID:" << my_id << "Avatar:" << my_avt << "Server Name:" << server_name << "Is Admin:" << is_admin << "Can Kick:" << can_kick;
    if (status == SUCCESS){
        if (reconnect_timer) {
            reconnect_timer->stop();
        }
        is_manual_disconnect = false;
        client.state = SUCCESS;
        client.user_id = my_id;
        client.user_avatar = my_avt;
        client.is_admin = is_admin;
        client.can_kick = can_kick;
        if (on_admin_status_changed) {
            on_admin_status_changed(is_admin);
        }
        if (!server_name.isEmpty()) {
            client.server_name = server_name;
        }
        if (!my_avt.isEmpty()) {
            user_request_file_download(0, my_avt, 0);
        }
        {
            QWriteLocker lock(&config.lock);
            bool found = false;
            for (server_t &server : config.server_list){
                if (server.address == client.server.address){
                    server = client.server;
                    found = true;
                    break;
                }
            }
            if (!found){
                config.server_list.append(client.server);
            }
        }
        write_config_async();
        if (on_server_list_changed) {
            on_server_list_changed();
        }
        if (on_connection_finish) {
            on_connection_finish();
        }
    }else{
        handle_connection_fail("Server rejected connection");
    }
}

inline void user_claim_admin_privilege(const QString &key){
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) {
        // printf("Failed to create ASN.1 element!\n");
        return;
    }
    asn1_write_value(element, "", "claim-admin-privilege", 1);
    asn1_write_string(element, "claim-admin-privilege.key", key);
    asn1_client_send_packet(element);
}

inline void receive_user_claim_admin_privilege(uint16_t status){
    // qDebug() << "claim admin privilege status:" << status;
    if (status == SUCCESS) {
        client.is_admin = true;
        client.can_kick = true;
        if (on_admin_status_changed) {
            on_admin_status_changed(true);
        }
    }
}

inline void user_request_my_info() {
    if (!asn1_client_is_connected()) return;
    asn1_node element;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "get-my-info", 1);
    asn1_write_value(element, "get-my-info.dummy", NULL, 0);
    asn1_client_send_packet(element);
}

inline void receive_my_info(asn1_node element) {
    user_info_t info;
    info.id = asn1_read_int(element, "my-info.id");
    info.uuid = asn1_read_string(element, "my-info.uuid");
    info.name = asn1_read_string(element, "my-info.name");
    info.avatar = asn1_read_string(element, "my-info.avatar");
    info.created_at = asn1_read_int(element, "my-info.created-at");
    info.server_uptime = asn1_read_int(element, "my-info.server-uptime");

    if (on_my_info_received) {
        on_my_info_received(info);
    }
}

inline bool user_upload_avatar(const QString &local_path) {
    if (!asn1_client_is_connected() || local_path.isEmpty()) return false;

    QFile file(local_path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    uint64_t total_size = file.size();
    if (total_size > 2 * 1024 * 1024) { // 2MB limit
        if (on_operation_error) on_operation_error("Avatar exceeds 2MB limit");
        return false;
    }

    QString upload_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString file_name = QFileInfo(local_path).fileName();

    const qint64 CHUNK_SIZE = 32768; // 32KB chunk
    uint64_t offset = 0;

    while (offset < total_size) {
        file.seek(offset);
        QByteArray chunk = file.read(CHUNK_SIZE);
        if (chunk.isEmpty()) break;

        asn1_node element;
        if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) break;

        asn1_write_value(element, "", "avatar-upload-chunk", 1);
        asn1_write_string(element, "avatar-upload-chunk.upload-id", upload_id);
        asn1_write_string(element, "avatar-upload-chunk.file-name", file_name);
        asn1_write_unsigned64(element, "avatar-upload-chunk.total-size", total_size);
        asn1_write_unsigned64(element, "avatar-upload-chunk.offset", offset);
        asn1_write_byte_array(element, "avatar-upload-chunk.data", chunk);

        asn1_client_send_packet(element);
        offset += chunk.size();
    }
    file.close();
    return true;
}

inline void user_poke_member(uint64_t target_user_id, const QString &message = "") {
    if (!asn1_client_is_connected() || target_user_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "poke-user", 1);
    asn1_write_unsigned64(element, "poke-user.target-user-id", target_user_id);
    if (!message.isEmpty()) {
        asn1_write_string(element, "poke-user.message", message);
    } else {
        asn1_write_value(element, "poke-user.message", NULL, 0);
    }
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_request_members() {
    if (!asn1_client_is_connected()) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "get-members-req", 1);
    asn1_write_value(element, "get-members-req.dummy", NULL, 0);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_request_member_detail(uint64_t user_id) {
    if (!asn1_client_is_connected() || user_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "get-member-detail-req", 1);
    asn1_write_unsigned64(element, "get-member-detail-req.user-id", user_id);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void receive_member_list(asn1_node element) {
    QList<member_item_t> members;
    int idx = 1;
    while (true) {
        QString prefix = QString("member_list.?%1").arg(idx);
        int len = 0;
        int ret = asn1_read_value(element, prefix.toUtf8().constData(), nullptr, &len);
        if (ret == ASN1_ELEMENT_NOT_FOUND) break;

        member_item_t m;
        m.id = asn1_read_int(element, (prefix + ".id").toUtf8().constData());
        m.name = asn1_read_string(element, (prefix + ".name").toUtf8().constData());
        m.avatar = asn1_read_string(element, (prefix + ".avatar").toUtf8().constData());
        m.is_online = asn1_read_bool(element, (prefix + ".is-online").toUtf8().constData());
        m.is_admin = asn1_read_bool(element, (prefix + ".is-admin").toUtf8().constData());
        members.append(m);
        idx++;
    }

    std::sort(members.begin(), members.end(), [](const member_item_t &a, const member_item_t &b) {
        return natural_sort_less_than(a.name, b.name);
    });

    if (on_member_list_received) {
        on_member_list_received(members);
    }
}

inline void receive_member_detail(asn1_node element) {
    user_info_t info;
    info.id = asn1_read_int(element, "member_detail.id");
    info.name = asn1_read_string(element, "member_detail.name");
    info.avatar = asn1_read_string(element, "member_detail.avatar");
    info.created_at = asn1_read_int(element, "member_detail.created-at");
    info.is_admin = (asn1_read_int(element, "member_detail.last-seen") == 1);
    QString raw_roles = asn1_read_string(element, "member_detail.roles");
    if (!raw_roles.trimmed().isEmpty()) {
        info.roles = raw_roles.split(',', Qt::SkipEmptyParts);
        for (QString &r : info.roles) {
            r = r.trimmed();
        }
    }
    if (on_member_detail_received) {
        on_member_detail_received(info);
    }
}

#endif // USER_H
