#ifndef ASN1_PARSER_H
#define ASN1_PARSER_H

#include <QString>
#include <QTcpSocket>
#include <QSslSocket>
#include <QtEndian>
#include <QMap>
#include <libtasn1.h>
#include <stdint.h>
// #include "utils/zlib_helper.h"
#include "utils/config.h"


struct asn1_client{
    QTcpSocket *socket;
    asn1_node definitions;
    uint state;
    server_t server;
    QMap<uint16_t, std::function<void(asn1_node)>> dispatch_table;

    void on(uint16_t tag, std::function<void(asn1_node)> f){
        dispatch_table[tag] = f;
    }
};

extern struct asn1_client client;

inline bool asn1_read_bool(asn1_node root, const char *name){
    char buffer[8] = {};
    int len = sizeof(buffer);

    int ret = asn1_read_value(root, name, buffer, &len);
    if (ret != ASN1_SUCCESS) return false;

    return QString::fromLatin1(buffer).compare("TRUE", Qt::CaseInsensitive) == 0;
}

inline QString asn1_read_string(asn1_node root, const char *name) {
    int len = 0;
    // Step 1: Get the required length
    int ret = asn1_read_value(root, name, NULL, &len);

    // If element doesn't exist or is empty
    if (ret != ASN1_MEM_ERROR && ret != ASN1_SUCCESS) return QString();
    if (len <= 0) return QString("");

    // Step 2: Allocate and Read
    QByteArray buffer(len, 0);
    ret = asn1_read_value(root, name, buffer.data(), &len);

    if (ret == ASN1_SUCCESS) {
        return QString::fromUtf8(buffer.data(), len);
    }
    return QString();
}

inline QByteArray asn1_read_bytes(asn1_node root, const char *name){
    int len = 0;
    int ret = asn1_read_value(root, name, nullptr, &len);

    if (ret != ASN1_MEM_ERROR && ret != ASN1_SUCCESS) return {};
    if (len <= 0) return {};

    QByteArray buffer(len, Qt::Uninitialized);
    ret = asn1_read_value(root, name, buffer.data(), &len);

    if (ret != ASN1_SUCCESS) return {};

    buffer.resize(len);
    return buffer;
}

inline uint64_t asn1_read_int(asn1_node root, const char *name) {
    int len = 0;
    int ret;
    ret = asn1_read_value(root, name, NULL, &len);
    if (len <= 0) return 0;

    QByteArray buffer(len, 0);
    unsigned char *ptr = (unsigned char*)buffer.data();
    ret = asn1_read_value(root, name, ptr, &len);
    if (ret != ASN1_SUCCESS) return 0;

    uint64_t val = 0;

    // Step 2: Handle Sign Extension
    // Only extend if the first bit is 1 AND it's not a 9-byte unsigned value
    if (len < 9 && (ptr[0] & 0x80)) {
        val = (uint64_t)-1; // Fill with 1s for negative numbers
    } else {
        val = 0; // Keep as 0 for positive numbers (including 9-byte ones)
    }

    // Step 3: Reconstruct
    for (int i = 0; i < len; i++) {
        val = (val << 8) | (ptr[i] & 0xFF);
    }

    return val;
}

inline void asn1_write_unsigned64(asn1_node element, const char* name, uint64_t value) {
    uint64_t be_val = qToBigEndian(value);
    unsigned char *bytes = (unsigned char *)&be_val;

    // If the highest bit of the data is 1, we MUST prepend 0x00
    // to keep it positive in ASN.1
    if (bytes[0] & 0x80) {
        unsigned char buffer[9];
        buffer[0] = 0x00;
        memcpy(&buffer[1], &be_val, 8);
        asn1_write_value(element, name, buffer, 9);
    } else {
        asn1_write_value(element, name, &be_val, 8);
    }
}

inline void asn1_write_signed64(asn1_node element, const char* name, int64_t value) {
    int64_t be_val = qToBigEndian((uint64_t)value);
    asn1_write_value(element, name, &be_val, 8);
}

inline int asn1_client_init(asn1_client &client, QString &asn_file) {
    char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
    client.socket = new QTcpSocket();
    client.definitions = NULL;
    int ret = asn1_parser2tree(asn_file.toUtf8(), &client.definitions, errorDesc);
    if (ret != ASN1_SUCCESS) printf("%s\n", errorDesc);
    return ret;
}

inline void asn1_client_send_packet(QTcpSocket *client, asn1_node element) {
    int ret;
    // DER Encoding
    char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
    int der_len = 0;

    ret = asn1_der_coding(element, "", NULL, &der_len, errorDesc);

    if (ret != ASN1_MEM_ERROR) {
        qDebug() << "Failed getting DER size:" << errorDesc;
        return;
    }

    QByteArray asn1_data(der_len, Qt::Uninitialized);
    ret = asn1_der_coding(element, "", asn1_data.data(), &der_len, errorDesc);
    if (ret != ASN1_SUCCESS) {
        qDebug() << "DER encoding failed:" << errorDesc;
        return;
    }

    asn1_data.resize(der_len);
    QByteArray packet;
    quint16 len = (quint16)asn1_data.size();
    char header[2];
    header[0] = (len >> 8) & 0xFF;
    header[1] = len & 0xFF;

    packet.append(header, 2);      // Add the 2-byte header
    packet.append(asn1_data);     // Add the raw uncompressed ASN.1 data

    client->write(packet);
    client->flush();
}

inline void asn1_client_read_response(asn1_client &client) {
    // Check if we have at least the 2-byte length header from Erlang {packet, 2}
    while (client.socket->bytesAvailable() >= 2) {
        char header[2];
        client.socket->peek(header, 2);

        quint16 len = (unsigned char)header[0] << 8 | (unsigned char)header[1];

        if (client.socket->bytesAvailable() < (len + 2)) return;

        client.socket->read(2); // Skip header
        QByteArray data = client.socket->read(len);

        asn1_node element = NULL;
        char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE];

        asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element);

        int ret = asn1_der_decoding(&element, data.constData(), data.size(), errorDesc);
        unsigned long tag_value;
        unsigned char class_value;
        int len2;

        ret = asn1_get_tag_der((const unsigned char*)data.constData(),
                               data.size(), &class_value, &len2, &tag_value);

        qDebug() << "Received";
        if (ret == ASN1_SUCCESS) {
            auto it = client.dispatch_table.find(tag_value);
            if (it != client.dispatch_table.end()){
                it.value()(element);
            }else{
                qDebug() << "No handler for tag:" << tag_value;
            }
        } else {
            qDebug() << "Decoding Error:" << errorDesc;
        }

        asn1_delete_structure(&element);
    }
}

#endif // ASN1_PARSER_H
