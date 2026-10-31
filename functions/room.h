#ifndef ROOM_H
#define ROOM_H

#include "utils/asn1_parser.h"
#include "common/types.h"
#include <functional>

inline std::function<void(const QList<room_t>&)> on_room_list_updated = nullptr;
inline std::function<void(uint64_t room_id, int join_type)> on_room_joined = nullptr;
inline std::function<void(uint64_t room_id, int join_type, const QString &err)> on_room_join_failed = nullptr;
inline std::function<void(uint64_t room_id, const QString &room_name)> on_room_chat_clicked = nullptr;
inline std::function<void(const chat_message_t &msg)> on_chat_message_received = nullptr;
inline std::function<void(uint64_t room_id, uint64_t msg_id)> on_message_deleted = nullptr;
inline std::function<void(uint64_t room_id, int count)> on_history_response = nullptr;

inline void user_delete_message(uint64_t room_id, uint64_t msg_id) {
    if (!asn1_client_is_connected() || msg_id == 0) return;
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "delete-msg", 1);
    asn1_write_unsigned64(element, "delete-msg.room-id", room_id);
    asn1_write_unsigned64(element, "delete-msg.msg-id", msg_id);
    asn1_client_send_packet(element);
}

inline void receive_delete_message(asn1_node element) {
    uint64_t room_id = asn1_read_int(element, "delete-msg.room-id");
    uint64_t msg_id = asn1_read_int(element, "delete-msg.msg-id");
    if (on_message_deleted) {
        on_message_deleted(room_id, msg_id);
    }
}

inline void user_create_room(const QString &name, const QString &password, uint limit, uint needed_voice_power = 0, uint needed_chat_power = 0, uint needed_speak_power = 0) {
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) {
        // printf("Failed to create ASN.1 element!\n");
        return;
    }
    asn1_write_value(element, "", "create-room", 1);
    asn1_write_string(element, "create-room.name", name);
    asn1_write_string(element, "create-room.password", password);
    asn1_write_unsigned64(element, "create-room.limit-clients", limit);
    asn1_write_unsigned64(element, "create-room.needed-voice-join-power", needed_voice_power);
    asn1_write_unsigned64(element, "create-room.needed-chat-join-power", needed_chat_power);
    asn1_write_unsigned64(element, "create-room.needed-speak-power", needed_speak_power);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_join_room(uint64_t room_id, const QString &password, int join_type = 0) {
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) {
        // printf("Failed to create ASN.1 element!\n");
        return;
    }
    asn1_write_value(element, "", "join-room", 1);
    asn1_write_unsigned64(element, "join-room.room-id", room_id);
    if (password.isEmpty()) {
        asn1_write_value(element, "join-room.password", NULL, 0);
    } else {
        asn1_write_string(element, "join-room.password", password);
    }
    asn1_write_unsigned64(element, "join-room.join-type", join_type);
    asn1_client_send_packet(element);
}

inline void user_update_server_settings(const QString &new_name, const QString &new_password) {
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) {
        // printf("Failed to create ASN.1 element!\n");
        return;
    }
    asn1_write_value(element, "", "update-server-settings", 1);
    asn1_write_string(element, "update-server-settings.server-name", new_name);
    asn1_write_string(element, "update-server-settings.server-password", new_password);
    asn1_client_send_packet(element);
}

inline void user_kick_member(uint64_t user_id, uint64_t room_id = 0) {
    if (!asn1_client_is_connected() || user_id == 0) return;
    asn1_node element = nullptr;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "kick-user", 1);
    asn1_write_unsigned64(element, "kick-user.user-id", user_id);
    if (room_id > 0) {
        asn1_write_unsigned64(element, "kick-user.room-id", room_id);
    } else {
        asn1_write_value(element, "kick-user.room-id", NULL, 0);
    }
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_update_room(uint64_t room_id, const QString &name, const QString &password, uint v_power = 0, uint c_power = 0, uint s_power = 0) {
    if (!asn1_client_is_connected() || room_id == 0) return;
    asn1_node element = nullptr;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "update-room", 1);
    asn1_write_unsigned64(element, "update-room.room-id", room_id);
    asn1_write_string(element, "update-room.name", name);
    asn1_write_string(element, "update-room.password", password);
    asn1_write_unsigned64(element, "update-room.needed-voice-join-power", v_power);
    asn1_write_unsigned64(element, "update-room.needed-chat-join-power", c_power);
    asn1_write_unsigned64(element, "update-room.needed-speak-power", s_power);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_send_chat_message(uint64_t room_id, const QString &content) {
    if (!asn1_client_is_connected() || content.trimmed().isEmpty()) return;
    ClientNetworkWorker *worker = client.network_worker;
    uint64_t sender_id = client.user_id;
    QString sender = config.username;
    bool queued = QMetaObject::invokeMethod(
        worker, [worker, room_id, content, sender_id, sender]() {
            asn1_node element = nullptr;
            int ret = asn1_create_element(
                client.definitions, "ChatProtocol.ProtocolMessage", &element);
            if (ret != ASN1_SUCCESS) {
                qWarning() << "Failed to create chat ASN.1 element.";
                return;
            }

            asn1_write_value(element, "", "chat", 1);
            asn1_write_unsigned64(element, "chat.room-id", room_id);
            asn1_write_unsigned64(element, "chat.msg-id", 0);
            asn1_write_unsigned64(element, "chat.sender-id", sender_id);
            asn1_write_string(element, "chat.sender", sender);
            asn1_write_string(element, "chat.sender-avatar", "");
            asn1_write_string(element, "chat.content", content);
            asn1_write_unsigned64(element, "chat.msg-type", MESSAGE_TYPE::TEXT);
            asn1_write_unsigned64(
                element, "chat.timestamp", QDateTime::currentSecsSinceEpoch());
            QByteArray packet = asn1_client_encode_packet(element);
            asn1_delete_structure(&element);
            if (!packet.isEmpty()) worker->send_packet(packet);
        }, Qt::QueuedConnection);
    if (!queued) {
        qWarning() << "Could not queue chat message for sending.";
        return;
    }
}

inline void user_leave_room(uint64_t room_id, int leave_type = 0) {
    if (!asn1_client_is_connected() || room_id == 0) return;
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "leave-room", 1);
    asn1_write_unsigned64(element, "leave-room.room-id", room_id);
    asn1_write_unsigned64(element, "leave-room.leave-type", leave_type);
    asn1_client_send_packet(element);
}

inline void user_request_history(uint64_t room_id, uint64_t before_id, int limit = 20) {
    if (!asn1_client_is_connected() || room_id == 0) return;
    asn1_node element;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    if (ret != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "history-req", 1);
    asn1_write_unsigned64(element, "history-req.room-id", room_id);
    asn1_write_unsigned64(element, "history-req.before-id", before_id);
    asn1_write_unsigned64(element, "history-req.limit", limit);
    asn1_client_send_packet(element);
}

inline void receive_chat_message(asn1_node element) {
    chat_message_t msg;
    msg.room_id = asn1_read_int(element, "chat.room-id");
    msg.id = asn1_read_int(element, "chat.msg-id");
    msg.sender_id = asn1_read_int(element, "chat.sender-id");
    msg.sender = asn1_read_string(element, "chat.sender");
    msg.sender_avatar = asn1_read_string(element, "chat.sender-avatar");
    msg.msg = asn1_read_string(element, "chat.content");
    msg.msg_type = asn1_read_int(element, "chat.msg-type");
    if (msg.msg_type == 0) msg.msg_type = MESSAGE_TYPE::TEXT;
    uint64_t ts = asn1_read_int(element, "chat.timestamp");
    msg.timestamp = (ts > 0) ? QDateTime::fromSecsSinceEpoch(ts) : QDateTime::currentDateTime();

    if (on_chat_message_received) {
        on_chat_message_received(msg);
    }
}

inline QList<room_t> parse_room_list(asn1_node element) {
    QList<room_t> rooms;
    int idx = 1;
    while (true) {
        QString item_prefix = QString("room_list.?%1").arg(idx);
        int len = 0;
        int ret = asn1_read_value(element, item_prefix.toUtf8().constData(), nullptr, &len);
        if (ret == ASN1_ELEMENT_NOT_FOUND) break;

        room_t room;
        room.id = asn1_read_int(element, (item_prefix + ".id").toUtf8().constData());
        room.name = asn1_read_string(element, (item_prefix + ".name").toUtf8().constData());
        room.has_password = asn1_read_bool(element, (item_prefix + ".has-password").toUtf8().constData());
        room.needed_voice_join_power = asn1_read_int(element, (item_prefix + ".needed-voice-join-power").toUtf8().constData());
        room.needed_chat_join_power = asn1_read_int(element, (item_prefix + ".needed-chat-join-power").toUtf8().constData());
        room.needed_speak_power = asn1_read_int(element, (item_prefix + ".needed-speak-power").toUtf8().constData());

        int u_idx = 1;
        while (true) {
            QString user_prefix = QString("%1.users-in-voice-room.?%2").arg(item_prefix).arg(u_idx);
            int u_len = 0;
            int u_ret = asn1_read_value(element, user_prefix.toUtf8().constData(), nullptr, &u_len);
            if (u_ret == ASN1_ELEMENT_NOT_FOUND) break;

            voice_user_t user;
            user.id = asn1_read_int(element, (user_prefix + ".id").toUtf8().constData());
            user.name = asn1_read_string(element, (user_prefix + ".name").toUtf8().constData());
            user.avatar = asn1_read_string(element, (user_prefix + ".avatar").toUtf8().constData());
            user.is_mute = asn1_read_bool(element, (user_prefix + ".is-mute").toUtf8().constData());
            user.is_deaf = asn1_read_bool(element, (user_prefix + ".is-deaf").toUtf8().constData());
            room.users.append(user);
            u_idx++;
        }

        std::sort(room.users.begin(), room.users.end(), [](const voice_user_t &a, const voice_user_t &b) {
            return natural_sort_less_than(a.name, b.name);
        });

        rooms.append(room);
        idx++;
    }
    return rooms;
}

inline void receive_room_list(asn1_node element) {
    QList<room_t> rooms = parse_room_list(element);
    // qDebug() << "Received room list, total:" << rooms.size();
    if (on_room_list_updated) {
        on_room_list_updated(rooms);
    }
}

#endif // ROOM_H
