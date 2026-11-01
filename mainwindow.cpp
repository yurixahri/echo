#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui/qcollapsewidget.h"
#include "utils/theme.h"
#include "utils/asn1_parser.h"
#include "functions/user.h"
#include "functions/file.h"
#include "ui/server_list.h"
#include "ui/connection_popup.h"
#include "ui/password_popup.h"
#include "ui/settings_popup.h"
#include "ui/toast.h"
#include "functions/sound.h"
#include <QPointer>
#include <QApplication>

struct theme global_theme;
QString theme_path = "default";
static QPointer<QPopup> active_loading_popup;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Echo");
    setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/assets/icons/logo_small.ico"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
    setMouseTracking(true);
    statusBar()->hide();

    ui->centralwidget->setMouseTracking(true);
    qApp->installEventFilter(this);

    parent_layout = new QVBoxLayout();
    main_splitter = new QSplitter(Qt::Horizontal);
    main_panel = new QWidget();
    main_panel->setObjectName("main_panel");
    QVBoxLayout *main_panel_layout = new QVBoxLayout(main_panel);
    main_panel_layout->setContentsMargins(6, 6, 6, 6);
    main_panel_layout->setSpacing(6);
    main_panel_layout->addWidget(main_splitter);

    ui->centralwidget->setLayout(parent_layout);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    parent_layout->setSpacing(0);

    create_title_bar();
    create_side_bar();
    create_main_area();

    {
        QReadLocker lock(&config.lock);
        if (config.window_width >= 400 && config.window_height >= 300) {
            resize(config.window_width, config.window_height);
        }
        if (!config.splitter_state.isEmpty()) {
            main_splitter->restoreState(config.splitter_state);
        }
        if (config.window_maximized) {
            showMaximized();
        }
    }

    connect(main_splitter, &QSplitter::splitterMoved, [this]() {
        QWriteLocker lock(&config.lock);
        config.splitter_state = main_splitter->saveState();
    });

    parent_layout->addWidget(main_panel, 1);
    set_ui_style();
    this->setFocus();

    on_theme_changed = [this](const QString &new_theme) {
        theme_path = new_theme;
        {
            QWriteLocker locker(&config.lock);
            config.theme = new_theme;
        }
        write_config_async();
        set_ui_style();
    };

    on_connection_start = [this](const QString &address) {
        client.server_name.clear();
        set_voice_panel_server_connected(voice_panel, false);
        if (server_content) {
            server_content->hide();
        }
        if (chat_area.widget) {
            chat_area.set_room_id(0);
            chat_area.widget->hide();
        }
        if (room_container) {
            update_room_view(room_container, {});
        }
        if (active_loading_popup) {
            active_loading_popup->deleteLater();
        }
        active_loading_popup = show_loading_popup(main_panel, "Connecting to " + address + "...");
    };

    on_connection_finish = [this]() {
        if (active_loading_popup) {
            active_loading_popup->deleteLater();
        }
        if (server_header.server_name) {
            server_header.server_name->setText(client.server_name.isEmpty() ? client.server.address : client.server_name);
        }
        if (server_content) {
            server_content->show();
        }
        set_voice_panel_server_connected(voice_panel, true);
    };

    on_connection_fail = [this](const QString &err) {
        if (active_loading_popup) {
            active_loading_popup->deleteLater();
        }
        if (server_content) {
            server_content->hide();
        }
        set_voice_panel_server_connected(voice_panel, false);
        if (chat_area.widget) {
            chat_area.set_room_id(0);
            chat_area.widget->hide();
        }
        if (room_container) {
            update_room_view(room_container, {});
        }
        show_toast(main_panel, err, "error");
    };

    on_disconnected = [this]() {
        stop_voice_channel(false);
        if (server_content) {
            server_content->hide();
        }
        set_voice_panel_server_connected(voice_panel, false);
        if (chat_area.widget) {
            chat_area.set_room_id(0);
            chat_area.widget->hide();
        }
        if (room_container) {
            update_room_view(room_container, {});
        }
        client.server_name.clear();
        if (server_header.server_name) {
            server_header.server_name->setText("");
        }
    };

    on_server_name_changed = [this](const QString &name) {
        client.server_name = name;
        if (server_header.server_name) {
            server_header.server_name->setText(name);
        }
        show_toast(main_panel, "Server name updated: " + name, "info");
    };

    on_operation_error = [this](const QString &err) {
        show_toast(main_panel, err, "error");
    };

    on_operation_success = [this](const QString &msg) {
        show_toast(main_panel, msg, "success");
    };

    on_user_kicked = [this](const QString &msg) {
        show_toast(main_panel, msg, "error");
    };

    on_chat_kicked = [this](uint64_t room_id, const QString &reason) {
        if (chat_area.widget && (room_id == 0 || chat_area.current_room_id() == room_id)) {
            chat_area.set_room_id(0);
            chat_area.widget->hide();
        }
        show_toast(main_panel, reason, "error");
    };

    on_user_poked = [this](const QString &sender, uint64_t sender_id, const QString &msg) {
        Q_UNUSED(sender_id);
        sound_play_poke();
        QString display = msg.isEmpty() ? QString("%1 poked you!").arg(sender)
                                        : QString("%1 poked you: %2").arg(sender, msg);
        show_toast(main_panel, display, "info");
    };

    on_server_password_required = [this](const QString &err) {
        if (active_loading_popup) {
            active_loading_popup->deleteLater();
        }
        open_server_password_popup(main_panel, err);
    };

    on_room_join_failed = [this](uint64_t room_id, int join_type, const QString &err) {
        if (room_id > 0) {
            client.server.room_passwords.remove(room_id);
            write_config_async();
        }
        open_room_password_popup(main_panel, room_id, get_room_name(room_id), join_type, err);
    };

    on_room_joined = [this](uint64_t room_id, int join_type) {
        if (join_type == 0) {
            chat_area_open_room(chat_area, room_id, get_room_name(room_id));
        } else if (join_type == 1) {
            start_voice_channel(room_id, get_room_name(room_id));
        }
    };

    on_voice_state_changed = [this](bool active, uint64_t room_id, const QString &room_name) {
        update_voice_panel_ui(voice_panel, active, room_id, room_name);
        for (room_t &r : current_rooms) {
            for (voice_user_t &u : r.users) {
                if (u.id == client.user_id) {
                    u.is_mute = voice_ui_state.is_mute;
                    u.is_deaf = voice_ui_state.is_deaf;
                }
            }
        }
        update_room_view(room_container, current_rooms);
    };
    on_active_voice_speakers_changed = [this](const QSet<uint64_t> &) {
        update_room_speaking_indicators(room_container);
    };

    on_room_chat_clicked = [this](uint64_t room_id, const QString &room_name) {
        chat_area_open_room(chat_area, room_id, room_name);
    };

    on_chat_message_received = [this](const chat_message_t &msg) {
        if (chat_area_enqueue_message(chat_area, msg)) {
            sound_play_message();
        }
    };

    on_runtime_avatar_ready = [this](const QString &avatar_path) {
        room_avatar_crop_cache.remove(avatar_path);
        chat_area_update_avatar(chat_area, avatar_path);
        update_room_view(room_container, current_rooms);
    };

    on_history_response = [this](uint64_t room_id, int count) {
        uint64_t active_room_id = chat_area.current_room_id();
        if (active_room_id == 0 || (room_id > 0 && room_id != active_room_id)) {
            return;
        }

        if (chat_area.widget) {
            chat_area_t *chat_ptr = (chat_area_t*)chat_area.widget->property("chat_area_ptr").value<void*>();
            if (chat_ptr) {
                if (count < 20) {
                    chat_ptr->has_more_history = false;
                }
                chat_area_flush_history_batch(*chat_ptr);
            }
        }
    };

    on_message_deleted = [this](uint64_t room_id, uint64_t msg_id) {
        if (chat_area.current_room_id() == room_id) {
            chat_area_remove_message(chat_area, msg_id);
        }
    };

    on_file_error = [this](const QString &err) {
        show_toast(main_panel, err, "error");
    };

    on_file_download_completed = [this](uint64_t room_id, const QString &file_path, const QString &local_abs_path) {
        Q_UNUSED(room_id);
        chat_area_handle_file_downloaded(chat_area, file_path, local_abs_path);
    };

    on_room_list_updated = [this](const QList<room_t> &rooms) {
        if (voice_ui_state.current_room_id > 0) {
            QSet<uint64_t> old_users;
            for (const room_t &r : current_rooms) {
                if (r.id == voice_ui_state.current_room_id) {
                    for (const voice_user_t &u : r.users) {
                        old_users.insert(u.id);
                    }
                    break;
                }
            }
            QSet<uint64_t> new_users;
            for (const room_t &r : rooms) {
                if (r.id == voice_ui_state.current_room_id) {
                    for (const voice_user_t &u : r.users) {
                        new_users.insert(u.id);
                    }
                    break;
                }
            }
            if (!old_users.isEmpty() && !new_users.isEmpty()) {
                for (uint64_t uid : new_users) {
                    if (uid != client.user_id && !old_users.contains(uid)) {
                        sound_play_voice_join();
                        break;
                    }
                }
                for (uint64_t uid : old_users) {
                    if (uid != client.user_id && !new_users.contains(uid)) {
                        sound_play_voice_leave();
                        break;
                    }
                }
            }
        }

        current_rooms = rooms;
        for (const room_t &room : rooms) {
            for (const voice_user_t &user : room.users) {
                if (!user.avatar.isEmpty() &&
                    !runtime_avatar_pixmaps.contains(user.avatar) &&
                    !runtime_avatar_buffers.contains(user.avatar)) {
                    user_request_file_download(0, user.avatar);
                }
            }
        }
        update_room_view(room_container, rooms);
        if (desired_voice_room_id > 0) {
            uint64_t target_rid = desired_voice_room_id;
            desired_voice_room_id = 0;
            for (const room_t &r : rooms) {
                if (r.id == target_rid) {
                    QString pwd = client.server.room_passwords.value(target_rid);
                    user_join_room(target_rid, pwd, 1);
                    break;
                }
            }
        }
        if (chat_area.widget && chat_area.widget->isVisible() && chat_area.current_room_id() > 0) {
            QString name = get_room_name(chat_area.current_room_id());
            if (chat_area.title) {
                chat_area.title->setText("# " + name);
            }
        }
    };
}

QString MainWindow::get_room_name(uint64_t room_id) const {
    for (const room_t &r : current_rooms) {
        if (r.id == room_id) return r.name;
    }
    return QString("Room %1").arg(room_id);
}

void MainWindow::set_ui_style(){
    load_theme(theme_path);
    if (title_bar) title_bar->setStyleSheet(get_title_bar_qss(theme_path));
    if (centralWidget()) centralWidget()->setStyleSheet(get_central_qss(theme_path));
    if (side_bar) side_bar->setStyleSheet(get_side_bar_qss(theme_path));
    if (main_area) main_area->setStyleSheet(get_main_area_qss(theme_path));
    if (chat_area.widget) {
        chat_area.widget->setStyleSheet(get_chat_area_qss(theme_path));
        chat_area_update_theme_icons(chat_area);
    }
    if (sidebar_logo) {
        sidebar_logo->setPixmap(colored_png(
            QCoreApplication::applicationDirPath() + "/assets/icons/logo.png",
            QColor(global_theme.foreground), QSize(100, 100)));
    }
    if (server_list_widget) {
        server_list_widget->update_theme_icons();
        update_server_list_ui(server_list_widget);
    }
    update_server_header_icons(server_header);
    update_voice_panel_icons(voice_panel);
    room_icon_cache.clear();
    if (room_container) {
        update_room_view(room_container, current_rooms);
    }
    for (QPopup *pop : findChildren<QPopup*>()) {
        if (pop->property("is_roles_popup").toBool()) {
            pop->setStyleSheet(get_roles_popup_qss(theme_path));
        } else {
            pop->setStyleSheet(get_popup_qss(theme_path));
        }
        for (QComboBox *cb : pop->findChildren<QComboBox*>()) {
            style_combobox(cb);
        }
    }
    for (QComboBox *cb : findChildren<QComboBox*>()) {
        style_combobox(cb);
    }
    for (QWidget *tc : findChildren<QWidget*>("toast_container")) {
        tc->setStyleSheet(get_toast_qss(theme_path));
    }
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
    minimize->setFocusPolicy(Qt::NoFocus);
    maximize->setFocusPolicy(Qt::NoFocus);
    close->setFocusPolicy(Qt::NoFocus);
    close->setObjectName("close");
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QPushButton::clicked, this, [this](){
        if (QWidget::isMaximized()){
            QWidget::showNormal();
        }else{
            QWidget::showMaximized();
        }
        QTimer::singleShot(50, [this]() {
            chat_area_update_media_sizes(chat_area);
        });
    });

    title_bar_layout->addWidget(title);
    title_bar_layout->addItem(gap);
    title_bar_layout->addWidget(minimize);
    title_bar_layout->addWidget(maximize);
    title_bar_layout->addWidget(close);

    title_bar->setLayout(title_bar_layout);
    parent_layout->addWidget(title_bar, 0);
}

void MainWindow::create_side_bar(){
    sidebar_logo = new QLabel();
    sidebar_logo->setPixmap(colored_png(QCoreApplication::applicationDirPath() + "/assets/icons/logo.png", QColor(global_theme.foreground), QSize(100, 100)));
    sidebar_logo->setAlignment(Qt::AlignCenter);

    QLineEdit *server_search_input = new QLineEdit();
    server_search_input->setPlaceholderText("Enter server adress");

    side_bar = new QWidget();
    QVBoxLayout *side_bar_layout = new QVBoxLayout();
    side_bar_layout->setContentsMargins(6, 6, 6, 6);
    side_bar_layout->setSpacing(6);
    server_list_widget = new QCollapseWidget("Server", "server.svg");
    server_list_widget->setObjectName("QCollapseWidget");
    update_server_list_ui(server_list_widget);
    on_server_list_changed = [this]() {
        update_server_list_ui(server_list_widget);
    };

    QCheckBox *is_ssl = new QCheckBox("SSL Connection");
    is_ssl->setChecked(false);

    QSpacerItem *gap = new QSpacerItem(
        0, 0,
        QSizePolicy::Minimum,
        QSizePolicy::Expanding
    );

    QLineEdit *username = new QLineEdit();
    username->setPlaceholderText("Enter your name");
    {
        QReadLocker lock(&config.lock);
        username->setText(config.username.isEmpty() ? "Anonymous" : config.username);
    }

    connect(username, &QLineEdit::editingFinished, [username]{
        QString new_name = username->text().trimmed();
        if (!new_name.isEmpty()) {
            {
                QWriteLocker lock(&config.lock);
                config.username = new_name;
            }
            write_config_async();
        }
    });

    QWidget *username_row = new QWidget();
    QHBoxLayout *username_layout = new QHBoxLayout(username_row);
    username_layout->setContentsMargins(0, 0, 0, 0);
    username_layout->setSpacing(6);

    sidebar_settings_btn = new QPushButton();
    sidebar_settings_btn->setObjectName("sidebar_settings_btn");
    sidebar_settings_btn->setCursor(Qt::PointingHandCursor);
    sidebar_settings_btn->setFixedSize(32, 32);
    sidebar_settings_btn->setToolTip("Settings");
    sidebar_settings_btn->setIcon(QIcon(colored_svg(
        QCoreApplication::applicationDirPath() + "/assets/icons/settings.svg",
        QColor(global_theme.foreground), QSize(18, 18))));
    connect(sidebar_settings_btn, &QPushButton::clicked, this, [this]() {
        open_settings_popup(main_panel);
    });

    username_layout->addWidget(username, 1);
    username_layout->addWidget(sidebar_settings_btn, 0);

    voice_panel = create_voice_panel(side_bar);

    side_bar_layout->addWidget(sidebar_logo);
    side_bar_layout->addWidget(server_search_input);
    side_bar_layout->addWidget(is_ssl);
    side_bar_layout->addWidget(server_list_widget);
    side_bar_layout->addItem(gap);
    side_bar_layout->addWidget(voice_panel.widget);
    side_bar_layout->addWidget(username_row);

    side_bar_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    side_bar->setLayout(side_bar_layout);
    side_bar->setAttribute(Qt::WA_StyledBackground, true);
    side_bar->setStyleSheet(get_side_bar_qss(theme_path));
    side_bar->setMinimumWidth(220);
    main_splitter->addWidget(side_bar);

    connect(server_search_input, &QLineEdit::returnPressed, [server_search_input, is_ssl]{
        server_t target{};
        target.address = server_search_input->text();
        target.is_ssl = is_ssl->isChecked();

        {
            QReadLocker lock(&config.lock);
            for (const server_t &server : config.server_list){
                if (server.address == target.address){
                    target.uuid = server.uuid;
                    target.password = server.password;
                    target.room_passwords = server.room_passwords;
                    target.user_volumes = server.user_volumes;
                    break;
                }
            }
        }

        start_connection(target);
    });
}

void MainWindow::create_main_area(){
    main_area = new QWidget();
    main_area->setObjectName("main_area");
    main_area->setAttribute(Qt::WA_StyledBackground, true);
    main_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    main_area->setMinimumWidth(300);

    QVBoxLayout *main_area_layout = new QVBoxLayout(main_area);
    main_area_layout->setContentsMargins(0, 0, 0, 0);
    main_area_layout->setSpacing(0);

    server_content = new QWidget(main_area);
    server_content->setObjectName("server_content");
    QVBoxLayout *server_content_layout = new QVBoxLayout(server_content);
    server_content_layout->setContentsMargins(0, 0, 0, 0);
    server_content_layout->setSpacing(0);

    server_header = create_server_header(server_content, main_panel);
    server_content_layout->addWidget(server_header.widget);

    QScrollArea *scroll = create_room_scroll_view(server_content, &room_container, main_panel);
    server_content_layout->addWidget(scroll, 1);

    main_area_layout->addWidget(server_content);

    // Hidden by default: main_area renders its background but has no inner content
    server_content->hide();

    main_splitter->addWidget(main_area);

    chat_area = create_chat_area(main_panel);
    chat_area.widget->setProperty("chat_area_ptr", QVariant::fromValue((void*)&chat_area));
    chat_area.widget->setStyleSheet(get_chat_area_qss(theme_path));
    chat_area.widget->setMinimumWidth(320);
    chat_area.widget->hide();
    main_splitter->addWidget(chat_area.widget);

    main_splitter->setChildrenCollapsible(false);
    main_splitter->setCollapsible(0, false);
    main_splitter->setCollapsible(1, false);
    main_splitter->setCollapsible(2, false);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 1);
}







// window event for resize and drag

void MainWindow::update_border_cursor(const QPoint &global_pos){
    if (isMaximized()) {
        unsetCursor();
        return;
    }

    QPoint pos = mapFromGlobal(global_pos);
    const int border = 6;
    bool left = pos.x() <= border && pos.x() >= 0;
    bool right = pos.x() >= width() - border && pos.x() <= width();
    bool top = pos.y() <= border && pos.y() >= 0;
    bool bottom = pos.y() >= height() - border && pos.y() <= height();

    if (rect().contains(pos)) {
        if ((left && top) || (right && bottom)) setCursor(Qt::SizeFDiagCursor);
        else if ((right && top) || (left && bottom)) setCursor(Qt::SizeBDiagCursor);
        else if (left || right) setCursor(Qt::SizeHorCursor);
        else if (top || bottom) setCursor(Qt::SizeVerCursor);
        else unsetCursor();
    } else {
        unsetCursor();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event){
    if (event->button() != Qt::LeftButton || isMaximized()) return;

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
    Q_UNUSED(event);
    update_border_cursor(QCursor::pos());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event){
    QWidget *w = qobject_cast<QWidget*>(obj);
    if (w && (w == this || w->window() == this)) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::HoverMove:
        case QEvent::Enter:
        case QEvent::Leave:
            update_border_cursor(QCursor::pos());
            break;
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
            if (mouse_event->button() == Qt::LeftButton && !isMaximized()) {
                QPoint pos = mapFromGlobal(mouse_event->globalPosition().toPoint());
                const int border = 6;
                Qt::Edges edges;
                if (pos.x() <= border) edges |= Qt::LeftEdge;
                if (pos.x() >= width() - border) edges |= Qt::RightEdge;
                if (pos.y() <= border) edges |= Qt::TopEdge;
                if (pos.y() >= height() - border) edges |= Qt::BottomEdge;

                bool is_input = false;
                for (QWidget *p = w; p && p != this; p = p->parentWidget()) {
                    if (p->inherits("QPushButton") || p->inherits("QLineEdit") ||
                        p->inherits("QTextEdit") || p->inherits("QPlainTextEdit")) {
                        is_input = true;
                        break;
                    }
                }

                if (edges != Qt::Edges() && !is_input) {
                    windowHandle()->startSystemResize(edges);
                    return true;
                }
            }
            if (obj == title_bar && mouse_event->button() == Qt::LeftButton) {
                windowHandle()->startSystemMove();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange) {
        {
            QWriteLocker lock(&config.lock);
            config.window_maximized = isMaximized();
        }
        QTimer::singleShot(50, [this]() {
            chat_area_update_media_sizes(chat_area);
        });
        QTimer::singleShot(200, [this]() {
            chat_area_update_media_sizes(chat_area);
        });
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!isMaximized() && !isMinimized()) {
        QWriteLocker lock(&config.lock);
        config.window_width = width();
        config.window_height = height();
        config.window_maximized = false;
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    {
        QWriteLocker lock(&config.lock);
        config.window_maximized = isMaximized();
        if (!isMaximized()) {
            config.window_width = width();
            config.window_height = height();
        }
        config.splitter_state = main_splitter->saveState();
    }
    write_config();
    clear_temporary_image_cache();
    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow(){
    clear_temporary_image_cache();
    on_voice_state_changed = nullptr;
    on_active_voice_speakers_changed = nullptr;
    voice_worker_shutdown(true);
    asn1_client_stop_network(true);
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    on_server_list_changed = nullptr;
    on_connection_start = nullptr;
    on_connection_finish = nullptr;
    on_connection_fail = nullptr;
    on_disconnected = nullptr;
    on_admin_status_changed = nullptr;
    on_operation_error = nullptr;
    on_server_password_required = nullptr;
    on_room_join_failed = nullptr;
    on_room_joined = nullptr;
    on_room_chat_clicked = nullptr;
    on_chat_message_received = nullptr;
    on_message_deleted = nullptr;
    on_history_response = nullptr;
    on_file_download_completed = nullptr;
    on_runtime_avatar_ready = nullptr;
    on_file_error = nullptr;
    on_room_list_updated = nullptr;
    delete ui;
}
