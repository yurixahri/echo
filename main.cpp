#include "mainwindow.h"

#include <QApplication>
#include "utils/asn1_parser.h"
#include "utils/config.h"
#include "functions/sound.h"
#include "functions/user.h"
#include "functions/room.h"
#include "functions/file.h"
#include "functions/role.h"
#include "common/types.h"

class GlobalFocusClearer : public QObject {
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        // Look for mouse clicks anywhere in the app
        if (event->type() == QEvent::MouseButtonPress) {
            QWidget *widget = qobject_cast<QWidget*>(obj);
            if (widget) {
                bool isInputWidget = false;
                for (QWidget *p = widget; p; p = p->parentWidget()) {
                    if (p->inherits("QLineEdit") ||
                        p->inherits("QTextEdit") ||
                        p->inherits("QPlainTextEdit") ||
                        p->inherits("QAbstractSpinBox") ||
                        p->inherits("QComboBox") ||
                        p->inherits("QPushButton")) {
                        isInputWidget = true;
                        break;
                    }
                }

                if (!isInputWidget) {
                    if (QApplication::focusWidget()) {
                        QApplication::focusWidget()->clearFocus();
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event); // Pass the event along normally
    }
};


struct asn1_client client;

int main(int argc, char *argv[]){
    QApplication a(argc, argv);
    a.setApplicationName("Echo");
    a.setApplicationDisplayName("Echo");
    a.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/assets/icons/logo_small.ico"));

    int ret;
    QString ans1_path = QCoreApplication::applicationDirPath()+"/ChatProtocol.asn";
    ret = asn1_client_init(client, ans1_path);
    if (ret != ASN1_SUCCESS) {
        // printf("Failed to load ASN.1 definitions!\n");
        return 1;
    }


    if (!get_config()){
        // printf("Failed to read config!\n");
        return 1;
    }
    theme_path = config.theme.isEmpty() ? "default" : config.theme;
    sound_init();
    voice_client.bitrate_kbps = config.voice_bitrate_kbps;
    voice_ui_state.bitrate_kbps = config.voice_bitrate_kbps;
    voice_client.input_volume = config.input_volume;
    voice_ui_state.input_volume = config.input_volume;
    voice_client.output_volume = config.output_volume;
    voice_ui_state.output_volume = config.output_volume;
    voice_client.vad_enabled = config.vad_enabled;
    voice_ui_state.vad_enabled = config.vad_enabled;
    voice_client.vad_threshold_db = config.vad_threshold_db;
    voice_ui_state.vad_threshold_db = config.vad_threshold_db;

    client.state = WAITTING_INIT;
    client.on(ASN1_EVENT::NEW_USER_CHALLENGE, do_challenge_proof);
    client.on(ASN1_EVENT::ROOM_LIST, receive_room_list);
    client.on(ASN1_EVENT::CHAT, receive_chat_message);
    client.on(ASN1_EVENT::DELETE_MSG, receive_delete_message);
    client.on(ASN1_EVENT::FILE_DOWNLOAD_CHUNK, receive_file_download_chunk);
    client.on(ASN1_EVENT::MY_INFO, receive_my_info);
    client.on(ASN1_EVENT::ROLE_LIST, receive_role_list);
    client.on(ASN1_EVENT::USER_ROLES_LIST, receive_user_roles_list);
    client.on(ASN1_EVENT::MEMBER_LIST, receive_member_list);
    client.on(ASN1_EVENT::MEMBER_DETAIL, receive_member_detail);
    client.on(ASN1_EVENT::RESPONSE, [](asn1_node element){
        uint16_t response_event = asn1_read_int(element, "response.event");
        uint16_t response_status = asn1_read_int(element, "response.status");
        QString response_msg = asn1_read_string(element, "response.message");

        // qDebug() << "response event:" << response_event << "status:" << response_status << "msg:" << response_msg;

        switch (response_event) {
            case ASN1_RESPONE_EVENT::RESPONSE_USER_CHECK_ID:
                if (response_status == PASSWORD_REQUIRED) {
                    if (on_server_password_required) {
                        on_server_password_required(response_msg.isEmpty() ? "Server password required" : response_msg);
                    }
                } else {
                    QStringList parts = response_msg.split(':');
                    uint64_t my_id = parts.value(0).toULongLong();
                    QString my_avt = parts.size() > 1 ? parts.value(1) : "";
                    QString srv_name = parts.size() > 2 ? parts.value(2) : "";
                    bool is_admin = parts.size() > 3 ? (parts.value(3).toInt() == 1) : false;
                    bool can_kick = parts.size() > 4 ? (parts.value(4).toInt() == 1) : is_admin;
                    receive_user_check_id(response_status, my_id, my_avt, srv_name, is_admin, can_kick);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_USER_CLAIM_ADMIN_PRIVILEGE:
                receive_user_claim_admin_privilege(response_status);
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Invalid admin setup token" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_CREATE_ROOM:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Failed to create room (Admin required)" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_JOIN_ROOM:
                if (response_status == SUCCESS) {
                    QStringList parts = response_msg.split(':');
                    uint64_t room_id = parts.value(0).toULongLong();
                    int join_type = parts.size() > 1 ? parts.value(1).toInt() : 0;
                    // qDebug() << "Successfully joined room:" << room_id << "type:" << join_type;
                    if (on_room_joined) on_room_joined(room_id, join_type);
                } else if (response_status == PASSWORD_REQUIRED) {
                    QStringList parts = response_msg.split(':');
                    uint64_t room_id = parts.value(0).toULongLong();
                    int join_type = parts.size() > 1 ? parts.value(1).toInt() : 0;
                    QString err = parts.size() > 2 ? parts.mid(2).join(':') : "Incorrect room password";
                    if (on_room_join_failed) {
                        on_room_join_failed(room_id, join_type, err);
                    }
                } else {
                    if (on_operation_error) {
                        on_operation_error(response_msg.isEmpty() ? "Failed to join room" : response_msg);
                    }
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_UPDATE_SERVER_SETTINGS:
                if (response_status == SUCCESS) {
                    if (!response_msg.isEmpty() && on_server_name_changed) {
                        on_server_name_changed(response_msg);
                    }
                } else if (on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Failed to update server settings" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_FILE_UPLOAD:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "File upload failed" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_HISTORY:
                if (on_history_response) {
                    QStringList parts = response_msg.split(':');
                    if (parts.size() > 1) {
                        on_history_response(parts.value(0).toULongLong(), parts.value(1).toInt());
                    } else {
                        on_history_response(0, response_msg.toInt());
                    }
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_DELETE_MSG:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Failed to delete message" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_AVATAR_UPLOAD:
                if (response_status == SUCCESS) {
                    if (on_avatar_updated) on_avatar_updated(response_msg);
                } else if (on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Avatar upload failed" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_CREATE_ROLE:
            case ASN1_RESPONE_EVENT::RESPONSE_UPDATE_ROLE_PERMS:
            case ASN1_RESPONE_EVENT::RESPONSE_DELETE_ROLE:
            case ASN1_RESPONE_EVENT::RESPONSE_ASSIGN_USER_ROLES:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Role operation failed" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_KICK_USER:
                if (response_status == SUCCESS) {
                    if (on_operation_success) on_operation_success(response_msg.isEmpty() ? "User kicked successfully" : response_msg);
                } else if (on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Failed to kick user" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_UPDATE_ROOM:
                if (response_status == SUCCESS) {
                    if (on_operation_success) on_operation_success(response_msg.isEmpty() ? "Room updated successfully" : response_msg);
                } else if (on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Failed to update room" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_KICKED:
                if (response_msg.startsWith("chat:")) {
                    QStringList parts = response_msg.split(':');
                    uint64_t kicked_room_id = parts.size() > 1 ? parts.value(1).toULongLong() : 0;
                    QString reason = parts.size() > 2 ? parts.mid(2).join(':') : response_msg;
                    if (on_chat_kicked) {
                        on_chat_kicked(kicked_room_id, reason.isEmpty() ? "Chat access revoked" : reason);
                    }
                } else {
                    QString reason = response_msg.startsWith("voice:") ? response_msg.mid(6) : response_msg;
                    stop_voice_channel(false);
                    if (on_user_kicked) {
                        on_user_kicked(reason.isEmpty() ? "You were kicked from the voice room" : reason);
                    }
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_PERMS_UPDATED: {
                QStringList parts = response_msg.split(':');
                bool is_admin = parts.value(0).toInt() == 1;
                bool can_kick = parts.value(1).toInt() == 1;
                client.is_admin = is_admin;
                client.can_kick = can_kick;
                if (on_admin_status_changed) {
                    on_admin_status_changed(is_admin);
                }
                if (on_operation_success) {
                    on_operation_success("Your permissions have been updated");
                }
                break;
            }
            case ASN1_RESPONE_EVENT::RESPONSE_POKE_USER:
                if (response_status == SUCCESS) {
                    if (on_operation_success) on_operation_success(response_msg.isEmpty() ? "User poked!" : response_msg);
                } else {
                    if (on_operation_error) on_operation_error(response_msg.isEmpty() ? "Poke failed" : response_msg);
                }
                break;
            case ASN1_RESPONE_EVENT::RESPONSE_POKED: {
                // message format: "sender_name:sender_id:custom_msg"
                QStringList parts = response_msg.split(':');
                QString sender_name = parts.value(0);
                uint64_t sender_id = parts.size() > 1 ? parts.value(1).toULongLong() : 0;
                QString custom_msg = parts.size() > 2 ? parts.mid(2).join(':') : "";
                if (on_user_poked) {
                    on_user_poked(sender_name, sender_id, custom_msg);
                }
                break;
            }
            case ASN1_RESPONE_EVENT::RESPONSE_CHAT:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Message failed to send" : response_msg);
                }
                break;
            default:
                if (response_status != SUCCESS && on_operation_error) {
                    on_operation_error(response_msg.isEmpty() ? "Operation failed" : response_msg);
                }
                break;
        }
    });

    GlobalFocusClearer *focus_clearer = new GlobalFocusClearer();
    a.installEventFilter(focus_clearer);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
