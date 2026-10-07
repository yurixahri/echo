#include "mainwindow.h"

#include <QApplication>
#include "utils/asn1_parser.h"
#include "utils/config.h"
#include "functions/user.h"
#include "common/types.h"


struct asn1_client client;

int main(int argc, char *argv[]){
    QApplication a(argc, argv);

    int ret;
    QString ans1_path = QCoreApplication::applicationDirPath()+"/ChatProtocol.asn";
    ret = asn1_client_init(client, ans1_path);
    if (ret != ASN1_SUCCESS) {
        printf("Failed to load ASN.1 definitions!\n");
        return 1;
    }


    if (!get_config()){
        printf("Failed to read config!\n");
        return 1;
    }

    printf("pass 1");
    client.state = WAITTING_INIT;
    client.on(ASN1_EVENT::NEW_USER_CHALLENGE, do_challenge_proof);
    client.on(ASN1_EVENT::RESPONSE_USER_CHECK_ID, receive_user_check_id);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
