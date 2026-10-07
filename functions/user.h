#ifndef USER_H
#define USER_H

#include "utils/asn1_parser.h"
#include "utils/config.h"
#include "common/types.h"

inline void connect_server(asn1_client client){
    QReadLocker lock(&config.lock);
    int ret;
    asn1_node element = NULL;
    ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);
    asn1_write_value(element, "", "client-check-id", 1);
    QByteArray uuid_utf8 = client.server.uuid.toUtf8();
    QByteArray name_utf8 = config.username.toUtf8();
    asn1_write_value(element, "client-check-id.uuid", uuid_utf8.data(), uuid_utf8.size());
    asn1_write_value(element, "client-check-id.name", name_utf8.data(), name_utf8.size());
    asn1_client_send_packet(client.socket, element);
}

inline bool check_difficulty(const QByteArray &hash, int difficulty){
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
    // asn1_print_structure(stdout, element, "", ASN1_PRINT_ALL);
    client.state = WAITTING_PROOF;
    QString uuid = asn1_read_string(element, "new-user-challenge.uuid");
    QByteArray challenge = asn1_read_bytes(element, "new-user-challenge.challenge");
    int difficulty = asn1_read_int(element, "new-user-challenge.difficulty");
    client.server.uuid = uuid;

    uint64_t solution = 0;
    while (true) {
        QByteArray data = uuid.toUtf8() + challenge + QByteArray::number(solution);
        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        if (check_difficulty(hash, difficulty)) break;
        ++solution;
    }

    asn1_node response = NULL;
    int ret = asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &response);
    asn1_write_value(response, "", "new-user-proof", 1);
    QByteArray utf8 = uuid.toUtf8();
    asn1_write_value(response, "new-user-proof.uuid", utf8.data(), utf8.size());
    asn1_write_unsigned64(response, "new-user-proof.solution", solution);
    asn1_client_send_packet(client.socket, response);
}

inline void receive_user_check_id(asn1_node element){
    bool is_approved = asn1_read_bool(element, "response-user-check-id.status");
    if (is_approved){
        client.state = SUCCESS;
        for (server_t &server : config.server_list){
            if (server.address == client.server.address)
                return;
        }
        {
            QWriteLocker lock(&config.lock);
            config.server_list.append(client.server);
        }
        write_config();
    }else{
        client.state = FAILED;
    }
    qDebug() << client.state;
}

#endif // USER_H
