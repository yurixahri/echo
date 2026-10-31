#ifndef ASN1_PARSER_H
#define ASN1_PARSER_H

#include <QString>
#include <QObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QThread>
#include <QMetaObject>
#include <QDebug>
#include <QtEndian>
#include <QMap>
#include <atomic>
#include <functional>
#include <libtasn1.h>
#include <stdint.h>
// #include "utils/zlib_helper.h"
#include "utils/config.h"

class ClientNetworkWorker : public QObject {
    Q_OBJECT

public:
    explicit ClientNetworkWorker(bool use_ssl, QObject *parent = nullptr)
        : QObject(parent), use_ssl(use_ssl) {}

public slots:
    void start(QString host, quint16 port) {
        if (use_ssl) {
            auto *ssl_socket = new QSslSocket(this);
            socket = ssl_socket;
            connect(ssl_socket, &QSslSocket::readyRead, this, &ClientNetworkWorker::read_available);
            connect(ssl_socket, &QSslSocket::connected, this, &ClientNetworkWorker::connected);
            connect(ssl_socket, &QSslSocket::disconnected, this, &ClientNetworkWorker::disconnected);
            connect(ssl_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
                emit network_error(socket ? socket->errorString() : QString("Connection error"));
            });
            ssl_socket->connectToHostEncrypted(host, port);
            return;
        }

        auto *tcp_socket = new QTcpSocket(this);
        socket = tcp_socket;
        connect(tcp_socket, &QTcpSocket::readyRead, this, &ClientNetworkWorker::read_available);
        connect(tcp_socket, &QTcpSocket::connected, this, &ClientNetworkWorker::connected);
        connect(tcp_socket, &QTcpSocket::disconnected, this, &ClientNetworkWorker::disconnected);
        connect(tcp_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            emit network_error(socket ? socket->errorString() : QString("Connection error"));
        });
        tcp_socket->connectToHost(host, port);
    }

    void send_packet(const QByteArray &packet) {
        if (!socket || !socket->isOpen()) {
            emit network_error("Cannot send data: not connected");
            return;
        }
        if (socket->write(packet) != packet.size()) {
            emit network_error(socket->errorString());
        }
    }

    void abort_connection() {
        if (socket) socket->abort();
    }

    void disconnect_from_host() {
        if (socket) socket->disconnectFromHost();
    }

    void acknowledge_packet() {
        if (pending_acknowledgements > 0) --pending_acknowledgements;
        read_available();
    }

signals:
    void connected();
    void disconnected();
    void network_error(const QString &error);
    void packet_received(const QByteArray &packet);

private slots:
    void read_available() {
        if (!socket) return;
        constexpr int max_pending_packets = 32;
        while (pending_acknowledgements < max_pending_packets) {
            if (packet_buffer.size() < 2) {
                packet_buffer.append(socket->read(2 - packet_buffer.size()));
                if (packet_buffer.size() < 2) return;
            }

            quint16 packet_size =
                (static_cast<quint8>(packet_buffer[0]) << 8) |
                static_cast<quint8>(packet_buffer[1]);
            qint64 expected_size = static_cast<qint64>(packet_size) + 2;
            if (packet_buffer.size() < expected_size) {
                packet_buffer.append(socket->read(expected_size - packet_buffer.size()));
                if (packet_buffer.size() < expected_size) return;
            }

            QByteArray packet = packet_buffer.mid(2, packet_size);
            packet_buffer.clear();
            ++pending_acknowledgements;
            emit packet_received(packet);
        }
    }

private:
    bool use_ssl = false;
    QTcpSocket *socket = nullptr;
    QByteArray packet_buffer;
    int pending_acknowledgements = 0;
};

struct asn1_client{
    asn1_node definitions;
    uint state;
    server_t server;
    QString server_name;
    uint64_t user_id = 0;
    QString user_avatar;
    bool is_admin = false;
    bool can_kick = false;
    QThread *network_thread = nullptr;
    ClientNetworkWorker *network_worker = nullptr;
    bool network_connected = false;
    std::atomic<uint64_t> network_generation{0};
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

    int ret = asn1_read_value(root, name, NULL, &len);

    if (ret != ASN1_MEM_ERROR && ret != ASN1_SUCCESS) return QString();
    if (len <= 0) return QString("");
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
    int ret = asn1_read_value(root, name, NULL, &len);
    if (ret != ASN1_MEM_ERROR && ret != ASN1_SUCCESS) return 0;
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

inline void asn1_write_string(asn1_node element, const char* name, QString value) {
    QByteArray utf8 = value.toUtf8();
    asn1_write_value(element, name, utf8.data(), utf8.size());
}

inline void asn1_write_byte_array(asn1_node element, const char* name, QByteArray value) {
    asn1_write_value(element, name, value.data(), value.size());
}

inline void asn1_write_bool(asn1_node element, const char* name, bool val) {
    asn1_write_value(element, name, val ? "TRUE" : "FALSE", 1);
}

inline int asn1_client_init(asn1_client &client, QString &asn_file) {
    char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = {};
    client.definitions = NULL;
    int ret = asn1_parser2tree(asn_file.toUtf8(), &client.definitions, errorDesc);
    // if (ret != ASN1_SUCCESS) printf("%s\n", errorDesc);
    return ret;
}

inline QByteArray asn1_client_encode_packet(asn1_node element) {
    int ret;
    // DER Encoding
    char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
    int der_len = 0;

    ret = asn1_der_coding(element, "", NULL, &der_len, errorDesc);

    if (ret != ASN1_MEM_ERROR) {
        // qDebug() << "Failed getting DER size:" << errorDesc;
        return {};
    }

    QByteArray asn1_data(der_len, Qt::Uninitialized);
    ret = asn1_der_coding(element, "", asn1_data.data(), &der_len, errorDesc);
    if (ret != ASN1_SUCCESS) {
        // qDebug() << "DER encoding failed:" << errorDesc;
        return {};
    }

    asn1_data.resize(der_len);
    QByteArray packet;
    quint16 len = (quint16)asn1_data.size();
    char header[2];
    header[0] = (len >> 8) & 0xFF;
    header[1] = len & 0xFF;

    packet.append(header, 2);      // Add the 2-byte header
    packet.append(asn1_data);     // Add the raw uncompressed ASN.1 data

    return packet;
}

inline bool asn1_client_is_connected() {
    return client.network_connected && client.network_worker;
}

inline void asn1_client_stop_network(bool wait_for_stop = false) {
    ++client.network_generation;
    ClientNetworkWorker *worker = client.network_worker;
    QThread *thread = client.network_thread;
    client.network_worker = nullptr;
    client.network_thread = nullptr;
    client.network_connected = false;

    if (!worker || !thread) return;

    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    if (!wait_for_stop) {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    bool queued = QMetaObject::invokeMethod(worker, [worker, thread]() {
        worker->abort_connection();
        thread->quit();
    }, Qt::QueuedConnection);
    if (!queued) {
        qWarning() << "Could not queue TCP worker shutdown.";
        thread->quit();
    }
    if (wait_for_stop) {
        thread->wait();
        delete thread;
    }
}

inline void asn1_client_send_packet(asn1_node element) {
    if (!client.network_worker || !client.network_connected) return;
    QByteArray packet = asn1_client_encode_packet(element);
    if (packet.isEmpty()) return;

    ClientNetworkWorker *worker = client.network_worker;
    bool queued = QMetaObject::invokeMethod(
        worker, [worker, packet]() { worker->send_packet(packet); },
        Qt::QueuedConnection);
    if (!queued) qWarning() << "Could not queue TCP packet for sending.";
}

inline void asn1_client_process_packet(const QByteArray &data) {
    asn1_node element = NULL;
    char errorDesc[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = {};

    if (asn1_create_element(client.definitions, "ChatProtocol.ProtocolMessage", &element) != ASN1_SUCCESS) {
        qWarning() << "Could not create ASN.1 protocol element.";
        return;
    }

    int ret = asn1_der_decoding(&element, data.constData(), data.size(), errorDesc);
    unsigned long tag_value = 0;
    unsigned char class_value = 0;
    int len2 = 0;

    if (ret != ASN1_SUCCESS) {
        qWarning() << "ASN.1 packet decoding failed:" << errorDesc;
        asn1_delete_structure(&element);
        return;
    }

    ret = asn1_get_tag_der(
        reinterpret_cast<const unsigned char *>(data.constData()), data.size(),
        &class_value, &len2, &tag_value);
    if (ret != ASN1_SUCCESS) {
        qWarning() << "Could not read ASN.1 packet tag.";
        asn1_delete_structure(&element);
        return;
    }

    auto it = client.dispatch_table.find(tag_value);
    if (it != client.dispatch_table.end()) {
        it.value()(element);
    } else {
        // qDebug() << "No handler for tag:" << tag_value;
    }

    asn1_delete_structure(&element);
}

#endif // ASN1_PARSER_H
