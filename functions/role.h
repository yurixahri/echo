#ifndef CLIENT_ROLE_H
#define CLIENT_ROLE_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include "common/types.h"
#include "utils/asn1_parser.h"

inline std::function<void(const QList<role_t> &roles)> on_roles_received = nullptr;
inline std::function<void(uint64_t user_id, const QList<user_role_item_t> &roles)> on_user_roles_received = nullptr;

inline void user_request_roles() {
    if (!asn1_client_is_connected()) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "get-roles-req", 1);
    asn1_write_unsigned64(element, "get-roles-req.dummy", 0);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_create_role(const QString &name) {
    if (!asn1_client_is_connected() || name.trimmed().isEmpty()) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "create-role", 1);
    asn1_write_string(element, "create-role.name", name.trimmed());
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_update_role_permissions(uint64_t role_id, const QString &perms_json) {
    if (!asn1_client_is_connected() || role_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "update-role-perms", 1);
    asn1_write_unsigned64(element, "update-role-perms.role-id", role_id);
    asn1_write_string(element, "update-role-perms.permissions", perms_json);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_delete_role(uint64_t role_id) {
    if (!asn1_client_is_connected() || role_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "delete-role", 1);
    asn1_write_unsigned64(element, "delete-role.role-id", role_id);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_request_user_roles(uint64_t user_id) {
    if (!asn1_client_is_connected() || user_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "get-user-roles-req", 1);
    asn1_write_unsigned64(element, "get-user-roles-req.user-id", user_id);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void user_assign_roles(uint64_t user_id, const QString &role_ids_csv) {
    if (!asn1_client_is_connected() || user_id == 0) return;
    asn1_node element = nullptr;
    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) return;
    asn1_write_value(element, "", "assign-user-roles", 1);
    asn1_write_unsigned64(element, "assign-user-roles.user-id", user_id);
    asn1_write_string(element, "assign-user-roles.role-ids", role_ids_csv);
    asn1_client_send_packet(element);
    asn1_delete_structure(&element);
}

inline void receive_role_list(asn1_node element) {
    QList<role_t> roles;
    int idx = 1;
    while (true) {
        QString prefix = QString("role-list.?%1").arg(idx);
        int len = 0;
        int ret = asn1_read_value(element, prefix.toUtf8().constData(), nullptr, &len);
        if (ret == ASN1_ELEMENT_NOT_FOUND) break;

        role_t role;
        role.id = asn1_read_int(element, (prefix + ".id").toUtf8().constData());
        role.name = asn1_read_string(element, (prefix + ".name").toUtf8().constData());
        QString perms_json = asn1_read_string(element, (prefix + ".permissions").toUtf8().constData());

        QJsonDocument doc = QJsonDocument::fromJson(perms_json.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                role.permissions[it.key()] = it.value().toInt();
            }
        }
        roles.append(role);
        idx++;
    }

    if (on_roles_received) {
        on_roles_received(roles);
    }
}

inline void receive_user_roles_list(asn1_node element) {
    uint64_t user_id = asn1_read_int(element, "user-roles-list.user-id");
    QString json_str = asn1_read_string(element, "user-roles-list.roles-json");

    QList<user_role_item_t> items;
    QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            user_role_item_t itm;
            itm.id = obj["id"].toInt();
            itm.name = obj["name"].toString();
            itm.assigned = obj["assigned"].toBool();
            items.append(itm);
        }
    }

    if (on_user_roles_received) {
        on_user_roles_received(user_id, items);
    }
}

#endif // CLIENT_ROLE_H
