#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui/qcollapsewidget.h"
#include "utils/theme.h"
#include "utils/asn1_parser.h"
#include "functions/user.h"

struct theme global_theme;
QString theme_path = "default";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setMouseTracking(true);
    statusBar()->hide();

    ui->centralwidget->setMouseTracking(true);

    parent_layout = new QVBoxLayout();
    main_splitter = new QSplitter(Qt::Horizontal);
    ui->centralwidget->setLayout(parent_layout);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    parent_layout->setSpacing(0);

    create_title_bar();
    create_side_bar();
    create_main_area();

    parent_layout->addWidget(main_splitter, 1);
    set_ui_style();
}

void MainWindow::set_ui_style(){
    load_theme(theme_path);
    title_bar->setStyleSheet(get_title_bar_qss(theme_path));
    centralWidget()->setStyleSheet(get_central_qss(theme_path));
}

void MainWindow::create_title_bar(){
    title_bar = new QWidget();
    QHBoxLayout *title_bar_layout = new QHBoxLayout();
    title_bar_layout->setContentsMargins(0, 0, 0, 0);
    title_bar_layout->setSpacing(0);

    QLabel *title = new QLabel("");
    QSpacerItem *gap = new QSpacerItem(
        0, 0,
        QSizePolicy::Expanding,
        QSizePolicy::Minimum
    );

    QPushButton *minimize = new QPushButton("-");
    QPushButton *maximize = new QPushButton("□");
    QPushButton *close = new QPushButton("x");
    close->setObjectName("close");
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QPushButton::clicked, this, [this](){
        if (QWidget::isMaximized()){
            QWidget::showNormal();
        }else{
            QWidget::showMaximized();
        }
    });

    title_bar_layout->addWidget(title);
    title_bar_layout->addItem(gap);
    title_bar_layout->addWidget(minimize);
    title_bar_layout->addWidget(maximize);
    title_bar_layout->addWidget(close);

    title_bar->setLayout(title_bar_layout);
    title_bar->installEventFilter(this);
    parent_layout->addWidget(title_bar, 0);
}

void MainWindow::create_side_bar(){
    QLabel *logo = new QLabel();
    logo->setPixmap(colored_png(QCoreApplication::applicationDirPath() + "/assets/icons/logo.png", QColor(global_theme.foreground), QSize(100, 100)));
    logo->setAlignment(Qt::AlignCenter);

    QLineEdit *server_search_input = new QLineEdit();
    server_search_input->setPlaceholderText("Enter server adress");

    side_bar = new QWidget();
    QVBoxLayout *side_bar_layout = new QVBoxLayout();
    QCollapseWidget *server_list = new QCollapseWidget("Server", "server.svg");
    server_list->setObjectName("QCollapseWidget");

    QCheckBox *is_ssl = new QCheckBox("SSL Connection");
    is_ssl->setChecked(false);

    side_bar_layout->addWidget(logo);
    side_bar_layout->addWidget(server_search_input);
    side_bar_layout->addWidget(is_ssl);
    side_bar_layout->addWidget(server_list);
    side_bar_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    side_bar_layout->setSpacing(10);
    side_bar->setLayout(side_bar_layout);
    side_bar->setStyleSheet(get_side_bar_qss(theme_path));
    side_bar->setMinimumWidth(200);
    main_splitter->addWidget(side_bar);

    connect(server_search_input, &QLineEdit::returnPressed, [server_search_input, is_ssl, &client, &config]{
        QReadLocker lock(&config.lock);
        delete client.socket;
        client.socket = nullptr;

        client.server = {};
        client.server.is_ssl = is_ssl->isChecked();
        QStringList args = server_search_input->text().split(":");
        uint16_t port = args.size() <= 1 ? (client.server.is_ssl ? 443 : 80) : args[1].toUInt();
        client.server.address = server_search_input->text();

        for (server_t &server : config.server_list){
            if (server.address == client.server.address){
                client.server = server;
                break;
            }
        }

        qDebug() << client.server.address;

        if (client.server.is_ssl) {
            QSslSocket *ssl_socket = new QSslSocket();
            client.socket = ssl_socket;
        } else {
            client.socket = new QTcpSocket();
        }

        QObject::connect(client.socket, &QTcpSocket::readyRead, [&client]() {
            asn1_client_read_response(client);
        });

        QObject::connect(client.socket, &QTcpSocket::connected, [&client]() {
            connect_server(client);
        });

        QObject::connect(client.socket, &QTcpSocket::disconnected, []() {
            qDebug() << "disconnected:";
        });
        QObject::connect(client.socket, &QTcpSocket::errorOccurred, [](QAbstractSocket::SocketError error) {
            qDebug() << "connect error:" << error;
        });

        if (client.server.is_ssl) {
            static_cast<QSslSocket *>(client.socket)->connectToHostEncrypted(args[0], port);
        } else {
            client.socket->connectToHost(args[0], port);
        }
    });
}
void MainWindow::create_main_area(){
    main_area = new QWidget();
    main_area->setObjectName("main_area");
    main_area->setStyleSheet(get_main_area_qss(theme_path));
    main_splitter->addWidget(main_area);
}


void MainWindow::mousePressEvent(QMouseEvent *event){
    if (event->button() != Qt::LeftButton) return;

    const int border = 6;
    QPoint pos = event->position().toPoint();

    Qt::Edges edges;

    if (pos.x() <= border) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - border) edges |= Qt::RightEdge;
    if (pos.y() <= border) edges |= Qt::TopEdge;
    if (pos.y() >= height() - border) edges |= Qt::BottomEdge;

    if (edges != Qt::Edges()) windowHandle()->startSystemResize(edges);
}


void MainWindow::mouseMoveEvent(QMouseEvent *event){
    const int border = 6;
    QPoint pos = event->position().toPoint();

    bool left = pos.x() <= border;
    bool right = pos.x() >= width() - border;
    bool top = pos.y() <= border;
    bool bottom = pos.y() >= height() - border;

    if ((left && top) || (right && bottom)) setCursor(Qt::SizeFDiagCursor);
    else if ((right && top) || (left && bottom)) setCursor(Qt::SizeBDiagCursor);
    else if (left || right) setCursor(Qt::SizeHorCursor);
    else if (top || bottom) setCursor(Qt::SizeVerCursor);
    else unsetCursor();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event){
    if (obj == title_bar && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);

        if (mouse_event->button() == Qt::LeftButton) {
            windowHandle()->startSystemMove();
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

MainWindow::~MainWindow(){
    delete ui;
}

