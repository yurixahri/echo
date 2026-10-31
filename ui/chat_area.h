#ifndef CHAT_AREA_H
#define CHAT_AREA_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QDateTime>
#include <QKeyEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QQueue>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>
#include <QMovie>
#include <QBuffer>
#include <QProcess>
#include <QImageReader>
#include <QMenu>
#include <QThreadPool>
#include "utils/theme.h"
#include "utils/config.h"
#include "utils/markdown_helper.h"
#include "functions/room.h"
#include "functions/file.h"
#include "ui/user_roles_popup.h"
#include "ui/user_info_popup.h"
#include "ui/toast.h"

struct chat_rate_limiter_t {
    QList<qint64> timestamps;
    qint64 timeout_until = 0;
};

inline chat_rate_limiter_t &get_chat_rate_limiter() {
    static chat_rate_limiter_t limiter;
    return limiter;
}

struct chat_area_t;
inline void chat_area_check_fill_viewport(chat_area_t &chat);

class ChatTextEdit : public QTextEdit {
public:
    std::function<bool(const QMimeData*)> on_mime_insert;
    explicit ChatTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}

    bool canInsertFromMimeData(const QMimeData *source) const override {
        if (source && (source->hasImage() || source->hasUrls())) return true;
        return QTextEdit::canInsertFromMimeData(source);
    }

    void insertFromMimeData(const QMimeData *source) override {
        if (on_mime_insert && on_mime_insert(source)) {
            return;
        }
        QTextEdit::insertFromMimeData(source);
    }
};

class ChatKeyFilter : public QObject {
public:
    std::function<void()> on_submit;
    std::function<bool()> on_paste;
    explicit ChatKeyFilter(QObject *parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & Qt::ShiftModifier)) {
                if (on_submit) on_submit();
                return true;
            }
            if (ke->matches(QKeySequence::Paste)) {
                if (on_paste && on_paste()) {
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

class ChatDropFilter : public QObject {
public:
    std::function<void(const QString &file_path)> on_file_dropped;
    explicit ChatDropFilter(QObject *parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData() && de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);
            de->acceptProposedAction();
            return true;
        } else if (event->type() == QEvent::Drop) {
            QDropEvent *de = static_cast<QDropEvent*>(event);
            if (de->mimeData() && de->mimeData()->hasUrls()) {
                QList<QUrl> urls = de->mimeData()->urls();
                if (!urls.isEmpty()) {
                    QString local_path = urls.first().toLocalFile();
                    if (!local_path.isEmpty() && on_file_dropped) {
                        on_file_dropped(local_path);
                        de->acceptProposedAction();
                        return true;
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

inline QSize calculate_media_display_size(const QString &local_path, int max_w, int max_h) {
    QImageReader reader(local_path);
    QSize sz = reader.size();
    if (!sz.isValid() || sz.isEmpty()) {
        QPixmap p(local_path);
        if (!p.isNull()) sz = p.size();
    }
    if (!sz.isValid() || sz.isEmpty()) {
        if (local_path.toLower().endsWith(".gif")) {
            QMovie m(local_path);
            if (m.isValid()) {
                m.jumpToFrame(0);
                sz = m.currentPixmap().size();
            }
        }
    }
    if (!sz.isValid() || sz.isEmpty()) {
        return QSize(qMin(max_w, 200), 150);
    }
    return sz.scaled(max_w, max_h, Qt::KeepAspectRatio);
}

inline QSize calculate_media_display_size_from_data(const QByteArray &data, int max_w, int max_h) {
    if (data.isEmpty()) return QSize(qMin(max_w, 200), 150);
    QBuffer buf(const_cast<QByteArray*>(&data));
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    QSize sz = reader.size();
    if (!sz.isValid() || sz.isEmpty()) {
        QPixmap p;
        if (p.loadFromData(data)) {
            sz = p.size();
        }
    }
    if (!sz.isValid() || sz.isEmpty()) {
        return QSize(qMin(max_w, 200), 150);
    }
    return sz.scaled(max_w, max_h, Qt::KeepAspectRatio);
}

inline void update_active_chat_gifs(QScrollArea *scroll_area, QWidget *messages_container) {
    if (!scroll_area || !messages_container || !scroll_area->viewport()) return;
    QRect view_rect = scroll_area->viewport()->rect();

    for (QLabel *lbl : messages_container->findChildren<QLabel*>("chat_msg_image")) {
        QMovie *m = lbl->movie();
        if (!m) continue;

        QPoint top_left = lbl->mapTo(scroll_area->viewport(), QPoint(0, 0));
        QRect lbl_rect(top_left, lbl->size());

        bool is_visible = view_rect.intersects(lbl_rect);
        if (is_visible) {
            if (m->state() == QMovie::Paused || m->state() == QMovie::NotRunning) {
                m->start();
            }
        } else {
            if (m->state() == QMovie::Running) {
                m->setPaused(true);
            }
        }
    }
}

inline void chat_area_update_media_sizes(QScrollArea *scroll_area, QWidget *messages_container) {
    if (!scroll_area || !messages_container) return;
    int vp_w = scroll_area->viewport() ? scroll_area->viewport()->width() : 300;
    int max_w = qMax(180, (int)(vp_w * 0.70));
    int max_media_w = qBound(160, (int)(vp_w * 0.30), 300);
    int max_media_h = qBound(160, (int)(vp_w * 0.40), 300);

    for (QLabel *lbl : messages_container->findChildren<QLabel*>("chat_msg_image")) {
        QString rel = lbl->property("file_path").toString();
        QString cached = get_cached_file_path(rel);
        if (!cached.isEmpty() && QFile::exists(cached)) {
            QSize sz = calculate_media_display_size(cached, max_media_w, max_media_h);
            lbl->setFixedSize(sz);
            if (lbl->movie()) {
                lbl->movie()->setScaledSize(sz);
            } else if (!lbl->pixmap().isNull()) {
                QPixmap orig(cached);
                lbl->setPixmap(orig.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            QWidget *bubble = lbl->parentWidget();
            if (bubble) {
                int bubble_w = sz.width() + 12;
                bubble->setProperty("ideal_w", bubble_w);
                bubble->setMaximumWidth(max_w);
                bubble->setFixedWidth(bubble_w);
            }
        }
    }

    for (QWidget *bubble : messages_container->findChildren<QWidget*>()) {
        if (bubble->objectName() == "chat_msg_bubble_me" || bubble->objectName() == "chat_msg_bubble_other") {
            bubble->setMaximumWidth(max_w);
            if (bubble->findChild<QLabel*>("chat_msg_image")) continue;

            int text_ideal_w = bubble->property("text_ideal_w").toInt();
            if (text_ideal_w > 0) {
                bubble->setFixedWidth(qMin(max_w, text_ideal_w));
            } else {
                int ideal_w = bubble->property("ideal_w").toInt();
                if (ideal_w > 0) {
                    bubble->setFixedWidth(qMin(max_w, ideal_w));
                }
            }
        }
    }
}

class ChatResizeFilter : public QObject {
public:
    QPointer<QScrollArea> scroll_area;
    QPointer<QWidget> messages_container;
    explicit ChatResizeFilter(QScrollArea *sa, QWidget *mc, QObject *parent = nullptr)
        : QObject(parent), scroll_area(sa), messages_container(mc) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Resize && scroll_area && messages_container) {
            chat_area_update_media_sizes(scroll_area, messages_container);
            QWidget *chat_widget = scroll_area->parentWidget();
            if (chat_widget) {
                chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
                if (c) chat_area_check_fill_viewport(*c);
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

class ImageClickFilter : public QObject {
public:
    QString file_path;
    explicit ImageClickFilter(const QString &path, QObject *parent = nullptr)
        : QObject(parent), file_path(path) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QWidget *w = qobject_cast<QWidget*>(obj);
            if (me->button() == Qt::LeftButton && !file_path.isEmpty()) {
                QString local_disk = get_cached_file_path(file_path);
                if (!local_disk.isEmpty()) {
                    open_local_file_detached(local_disk);
                } else if (runtime_chat_image_data.contains(file_path)) {
                    QString tmp = get_temp_preview_dir() + "/" + QFileInfo(file_path).fileName();
                    QFile f(tmp);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(runtime_chat_image_data[file_path]);
                        f.close();
                        open_local_file_detached(tmp);
                    }
                }
                return true;
            } else if (me->button() == Qt::RightButton && !file_path.isEmpty() && w) {
                QMenu menu(w);
                menu.setObjectName("chat_context_menu");

                QAction *copy_act = menu.addAction("Copy Image");
                QString p = file_path;
                QObject::connect(copy_act, &QAction::triggered, [p]() {
                    copy_image_to_clipboard(runtime_chat_image_data.value(p), get_cached_file_path(p));
                });

                QAction *save_act = menu.addAction("Save Image As...");
                QObject::connect(save_act, &QAction::triggered, [p, w]() {
                    QString fn = QFileInfo(p).fileName();
                    QString dest = QFileDialog::getSaveFileName(w->window(), "Save Image As", fn);
                    if (!dest.isEmpty()) {
                        QByteArray d = runtime_chat_image_data.value(p);
                        if (!d.isEmpty()) {
                            QFile f(dest);
                            if (f.open(QIODevice::WriteOnly)) {
                                f.write(d);
                                f.close();
                            }
                        } else {
                            QString cached = get_cached_file_path(p);
                            if (!cached.isEmpty()) {
                                QFile::remove(dest);
                                QFile::copy(cached, dest);
                            }
                        }
                    }
                });

                QAction *open_act = menu.addAction("Open in Default App");
                QObject::connect(open_act, &QAction::triggered, [p]() {
                    QString local_disk = get_cached_file_path(p);
                    if (!local_disk.isEmpty()) {
                        open_local_file_detached(local_disk);
                    } else if (runtime_chat_image_data.contains(p)) {
                        QString tmp = get_temp_preview_dir() + "/" + QFileInfo(p).fileName();
                        QFile f(tmp);
                        if (f.open(QIODevice::WriteOnly)) {
                            f.write(runtime_chat_image_data[p]);
                            f.close();
                            open_local_file_detached(tmp);
                        }
                    }
                });

                menu.exec(me->globalPosition().toPoint());
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

struct pending_attachment_t {
    QString local_path;
    QString filename;
    uint64_t size = 0;
    uint16_t msg_type = MESSAGE_TYPE::OTHER;
};

struct chat_area_t {
    QWidget *parent = nullptr;
    QWidget *widget = nullptr;
    QLabel *title = nullptr;
    QPushButton *close_btn = nullptr;
    QScrollArea *scroll_area = nullptr;
    QWidget *messages_container = nullptr;
    QVBoxLayout *messages_layout = nullptr;
    QPushButton *new_messages_bar = nullptr;
    QWidget *preview_container = nullptr;
    QLabel *preview_content = nullptr;
    QWidget *pending_container = nullptr;
    QLabel *pending_icon = nullptr;
    QLabel *pending_name = nullptr;
    QPushButton *pending_clear_btn = nullptr;
    QPushButton *bold_btn = nullptr;
    QPushButton *italic_btn = nullptr;
    QPushButton *underline_btn = nullptr;
    QPushButton *file_btn = nullptr;
    QPushButton *emoji_btn = nullptr;
    ChatTextEdit *input = nullptr;
    QString current_room_name;
    pending_attachment_t pending_file;
    uint64_t oldest_msg_id = 0;
    uint64_t newest_msg_id = 0;
    bool has_more_history = true;
    bool is_loading_history = false;
    QList<chat_message_t> pending_history_batch;
    QString last_date_key;
    QWidget *last_group_widget = nullptr;
    QVBoxLayout *last_bubbles_layout = nullptr;
    uint64_t last_group_sender_id = (uint64_t)-1;
    QList<QPointer<QLabel>> sliding_window_images;
    int last_scroll_val = 0;

    uint64_t current_room_id() const {
        return widget ? widget->property("room_id").toULongLong() : 0;
    }
    void set_room_id(uint64_t id) {
        if (!widget) return;
        if (current_room_id() != id) {
            sliding_window_images.clear();
            pending_history_batch.clear();
            last_date_key.clear();
            last_group_widget = nullptr;
            last_bubbles_layout = nullptr;
            last_group_sender_id = (uint64_t)-1;
            if (id == 0) is_loading_history = false;
        }
        widget->setProperty("room_id", (qulonglong)id);
    }
};

inline void adjust_input_height(QTextEdit *edit, int min_h = 36, int max_h = 130) {
    if (!edit || !edit->document()) return;
    int doc_h = (int)edit->document()->size().height();
    int h = qBound(min_h, doc_h + 8, max_h);
    if (edit->height() != h) {
        edit->setFixedHeight(h);
    }
}

inline void chat_format_selection(QTextEdit *edit, const QString &before, const QString &after) {
    if (!edit) return;
    QTextCursor cursor = edit->textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText(before + selected + after);
    } else {
        int pos = cursor.position();
        cursor.insertText(before + after);
        cursor.setPosition(pos + before.length());
        edit->setTextCursor(cursor);
    }
    edit->setFocus();
}

static const QStringList standard_emojis = {
    // --- Smileys & Faces ---
    "😀", "😃", "😄", "😁", "😆", "😅", "😂", "🤣", "😊",
    "😇", "🙂", "🙃", "😉", "😌", "😍", "🥰", "😘", "😗", "😙",
    "😚", "😋", "😛", "😝", "😜", "🤪", "🤨", "🧐", "🤓", "😎",
    "🤩", "🥳", "😏", "😒", "😞", "😔", "😟", "😕", "🙁",
    "☹️", "😣", "😖", "😫", "😩", "🥺", "😢", "😭", "😤", "😠",
    "😡", "🤬", "🤯", "😳", "🥵", "🥶", "😱", "😨", "😰", "😥",
    "😓", "🤭", "🤫", "🤥", "😶", "😶‍🌫️",
    "😐", "😑", "😬", "🥱", "😴", "🤤", "😪", "😮‍💨", "😵",
    "😵‍💫", "🤐", "🥴", "🤢", "🤮", "🤧", "😷", "🤒", "🤕",

    // --- Gestures & Body ---
    "👋", "🤚", "🖐️", "✋", "🖖", "👌", "🤏", "✌️", "🤞",
    "🤟", "🤘", "🤙", "👈", "👉", "👆", "🖕", "👇", "☝️",
    "👍", "👎", "✊", "👊", "🤛", "🤜", "👏", "🙌", "👐", "🤲",
    "🤝", "🙏", "✍️", "💅", "🤳", "💪", "🦾", "🦿", "🦵", "🦶",
    "👂", "🦻", "👃", "🧠", "🦷", "🦴", "👀", "👁️",

    // --- Hearts & Clothing ---
    "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤",
    "🟤", "⚫", "⚪", "🤍", "🤎", "💔", "❤️‍🔥", "❤️‍🩹", "❣️", "💕",
    "💞", "💓", "💗", "💖", "💘", "💝", "💟", "🧦",
    "👑", "👒", "🎩", "🎓", "🧢", "⛑️", "📿", "💄", "💍",

    // --- Flowers & Plants ---
    "💐", "🌸", "💮", "🌹", "🥀", "🌺", "🌻", "🌼", "🌷",
    "🌱", "🌲", "🌳", "🌴", "🌵", "🌾", "🌿", "🍀",
    "🍁", "🍂", "🍃", "🍄", "🌰", "🎋", "🎍",

    // --- Animals & Nature ---
    "🐶", "🐱", "🐭", "🐹", "🐰", "🦊", "🐻", "🐼", "🐻‍❄️", "🐨",
    "🐯", "🦁", "🐮", "🐷", "🐽", "🐸", "🐵", "🙈", "🙉", "🙊",
    "🐒", "🐔", "🐧", "🐦", "🐤", "🐣", "🐥", "🦆", "🦅", "🦉",
    "🦇", "🐺", "🐗", "🐴", "🦄", "🐝", "🐛", "🦋", "🐌",
    "🐞", "🐜", "🦂", "🕸️", "🕷️", "🐢", "🐍",
    "🦎", "🦖", "🦕", "🐙", "🦑", "🦐", "🦞", "🦀", "🐡", "🐠",
    "🐟", "🐬", "🐳", "🐋", "🦈", "🐊", "🐅", "🐆", "🦓", "🦍",

    // --- Food & Drink ---
    "🍏", "🍎", "🍐", "🍊", "🍋", "🍌", "🍉", "🍇", "🍓",
    "🍈", "🍒", "🍑", "🥭", "🍍", "🥥", "🥝", "🍅", "🍆", "🥑",
    "🥦", "🥬", "🥒", "🌶️", "🌽", "🥕", "🧄", "🧅",
    "🥔", "🍠", "🥐", "🥯", "🍞", "🥖", "🥨", "🧀", "🥞", "🧇",
    "🥓", "🥩", "🍗", "🍖", "🌭", "🍔", "🍟", "🍕", "🥪",
    "🌮", "🌯", "🥙", "🧆", "🥚", "🍳", "🥘", "🍲",
    "🥣", "🥗", "🍿", "🧈", "🧂", "🥫", "🍱", "🍘", "🍙", "🍚",

    // --- Travel & Places ---
    "🚗", "🚕", "🚙", "🚌", "🚎", "🏎️", "🚓", "🚑", "🚒", "🚐",
    "🛻", "🚚", "🚛", "🚜", "🛵", "🏍️", "🛺", "🚲", "🛴", "🛹",
    "🚏", "⛽", "🚨", "🚥", "🚦", "🚧", "⚓", "⛵", "🛶", "🚤",
    "🛳️", "⛴️", "🚢", "✈️", "🛩️", "🛫", "🛬", "🪂", "💺", "🚁",
    "🚂", "🚃", "🚄", "🚅", "🚆", "🚇", "🌋", "🗻",

    // --- Activities & Sports ---
    "⚽", "🏀", "🏈", "⚾", "🥎", "活", "🏐", "🏉", "🥏", "🏓",
    "🏸", "🏒", "🏑", "🥍", "🏏", "🥅", "⛳", "🪁", "🏹",
    "🎣", "🤿", "🥊", "🥋", "🎽", "🛹", "🛷", "🥌", "🎿",
    "⛷️", "🏂", "🪂", "🏋️", "🤼", "🤸", "⛹️", "🤺", "🤾", "🏌️"
};

// ponytail: hardcoded standard emoji set; upgrade to category tabs or dynamic search if custom emojis added.
inline void open_emoji_picker(QWidget *parent, QWidget *anchor, QTextEdit *target_input) {
    if (!parent || !anchor || !target_input) return;

    QFrame *popup = new QFrame(parent->window(), Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName("emoji_picker_popup");
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setStyleSheet(get_emoji_picker_qss(theme_path));

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Emojis", popup);
    title->setObjectName("popup_section_title");
    layout->addWidget(title);

    QScrollArea *scroll = new QScrollArea(popup);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *grid_container = new QWidget();
    QGridLayout *grid = new QGridLayout(grid_container);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    int row = 0;
    int col = 0;
    const int max_cols = 7;

    for (const QString &emoji : standard_emojis) {
        QPushButton *btn = new QPushButton(emoji, grid_container);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setToolTip(emoji);

        QObject::connect(btn, &QPushButton::clicked, [target_input, emoji, popup]() {
            if (target_input) {
                target_input->insertPlainText(emoji);
                target_input->setFocus();
            }
            popup->close();
        });

        grid->addWidget(btn, row, col);
        col++;
        if (col >= max_cols) {
            col = 0;
            row++;
        }
    }

    scroll->setWidget(grid_container);
    layout->addWidget(scroll);

    int popup_w = 236;
    int popup_h = 240;
    popup->resize(popup_w, popup_h);

    QPoint global_btn_pos = anchor->mapToGlobal(QPoint(0, 0));
    QPoint target_pos = global_btn_pos - QPoint(0, popup_h + 4);
    if (target_pos.y() < 0) {
        target_pos = global_btn_pos + QPoint(0, anchor->height() + 4);
    }
    if (QScreen *screen = QGuiApplication::screenAt(global_btn_pos)) {
        int right_limit = screen->geometry().right();
        if (target_pos.x() + popup_w > right_limit) {
            target_pos.setX(right_limit - popup_w - 8);
        }
    }
    popup->move(target_pos);
    popup->show();
}

inline void chat_area_set_pending_file(chat_area_t &chat, const QString &file_path) {
    if (file_path.isEmpty()) return;
    QFileInfo fi(file_path);
    if (!fi.exists() || !fi.isFile()) return;

    if (fi.size() > 10 * 1024 * 1024) { // 10MB limit
        if (on_file_error) {
            on_file_error(QString("File exceeds 10MB limit (size: %1 MB)").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1));
        }
        return;
    }

    chat.pending_file.local_path = file_path;
    chat.pending_file.filename = fi.fileName();
    chat.pending_file.size = fi.size();
    chat.pending_file.msg_type = detect_file_msg_type(file_path);

    QString size_str;
    if (fi.size() < 1024) {
        size_str = QString("%1 B").arg(fi.size());
    } else if (fi.size() < 1024 * 1024) {
        size_str = QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1);
    } else {
        size_str = QString("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1);
    }

    if (chat.pending_name) {
        chat.pending_name->setText(QString("%1 (%2)").arg(chat.pending_file.filename, size_str));
    }

    if (chat.pending_icon) {
        if (chat.pending_icon->movie()) {
            chat.pending_icon->movie()->stop();
            chat.pending_icon->setMovie(nullptr);
        }
        if (chat.pending_file.msg_type == MESSAGE_TYPE::IMAGE) {
            if (file_path.toLower().endsWith(".gif")) {
                QMovie *movie = new QMovie(file_path, QByteArray(), chat.pending_icon);
                movie->setScaledSize(QSize(32, 32));
                chat.pending_icon->setMovie(movie);
                movie->start();
            } else {
                QPixmap pix(file_path);
                if (!pix.isNull()) {
                    chat.pending_icon->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    chat.pending_icon->setText("🖼️");
                }
            }
        } else if (chat.pending_file.msg_type == MESSAGE_TYPE::VIDEO) {
            chat.pending_icon->setText("🎬");
        } else {
            chat.pending_icon->setText("📄");
        }
    }

    if (chat.pending_container) {
        chat.pending_container->show();
    }
}

inline void chat_area_clear_pending_file(chat_area_t &chat) {
    chat.pending_file = pending_attachment_t{};
    if (chat.pending_icon && chat.pending_icon->movie()) {
        chat.pending_icon->movie()->stop();
        chat.pending_icon->setMovie(nullptr);
    }
    if (chat.pending_container) {
        chat.pending_container->hide();
    }
}

inline void setup_file_card_actions(QWidget *file_card, const QString &content, const QString &cached, bool is_video, uint64_t room_id) {
    if (!file_card) return;
    QWidget *btn_row = file_card->findChild<QWidget*>("chat_file_btn_row");
    if (!btn_row) return;

    QLayout *lay = btn_row->layout();
    while (lay->count() > 0) {
        QLayoutItem *it = lay->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    QString filename = QFileInfo(content).fileName();

    if (!cached.isEmpty() && QFile::exists(cached)) {
        if (is_video) {
            QPushButton *play_btn = new QPushButton("Play", btn_row);
            play_btn->setObjectName("chat_open_file_btn");
            play_btn->setCursor(Qt::PointingHandCursor);
            play_btn->setMinimumWidth(54);
            QObject::connect(play_btn, &QPushButton::clicked, [cached]() {
                open_local_file_detached(cached);
            });
            lay->addWidget(play_btn);
        } else {
            QPushButton *open_btn = new QPushButton("Open", btn_row);
            open_btn->setObjectName("chat_open_file_btn");
            open_btn->setCursor(Qt::PointingHandCursor);
            open_btn->setMinimumWidth(54);
            QObject::connect(open_btn, &QPushButton::clicked, [cached]() {
                open_local_file_detached(cached);
            });
            lay->addWidget(open_btn);
        }

        QPushButton *save_btn = new QPushButton("Save As...", btn_row);
        save_btn->setObjectName("chat_save_file_btn");
        save_btn->setCursor(Qt::PointingHandCursor);
        save_btn->setMinimumWidth(64);
        QObject::connect(save_btn, &QPushButton::clicked, [cached, filename, file_card]() {
            QString dest = QFileDialog::getSaveFileName(file_card ? file_card->window() : nullptr,
                                                        "Save File As", filename);
            if (!dest.isEmpty()) {
                QFile::remove(dest);
                QFile::copy(cached, dest);
            }
        });
        lay->addWidget(save_btn);

        QPushButton *folder_btn = new QPushButton("Folder", btn_row);
        folder_btn->setObjectName("chat_save_file_btn");
        folder_btn->setCursor(Qt::PointingHandCursor);
        folder_btn->setMinimumWidth(54);
        folder_btn->setToolTip("Open in Directory");
        QObject::connect(folder_btn, &QPushButton::clicked, [cached]() {
            show_in_file_manager(cached);
        });
        lay->addWidget(folder_btn);
    } else {
        QPushButton *down_btn = new QPushButton("Download", btn_row);
        down_btn->setObjectName("chat_open_file_btn");
        down_btn->setCursor(Qt::PointingHandCursor);
        down_btn->setMinimumWidth(64);
        QObject::connect(down_btn, &QPushButton::clicked, [down_btn, room_id, content]() {
            down_btn->setText("Downloading...");
            down_btn->setEnabled(false);
            user_request_file_download(room_id, content, 0);
        });
        lay->addWidget(down_btn);
    }
}

inline QString format_chat_date(const QDate &date) {
    return date.toString("MMMM d, yyyy");
}

inline QWidget* create_chat_date_separator(const QDate &date) {
    QWidget *sep = new QWidget();
    sep->setObjectName("chat_date_separator");
    sep->setProperty("date_key", date.toString("yyyy-MM-dd"));

    QHBoxLayout *lay = new QHBoxLayout(sep);
    lay->setContentsMargins(6, 10, 6, 6);
    lay->setSpacing(10);

    QFrame *l1 = new QFrame(sep); l1->setObjectName("chat_date_line");
    QLabel *lbl = new QLabel(format_chat_date(date), sep); lbl->setObjectName("chat_date_label");
    QFrame *l2 = new QFrame(sep); l2->setObjectName("chat_date_line");

    lay->addWidget(l1, 1);
    lay->addWidget(lbl, 0);
    lay->addWidget(l2, 1);

    return sep;
}

inline void chat_area_unload_image(chat_area_t &chat, QLabel *lbl) {
    Q_UNUSED(chat);
    if (!lbl || !lbl->property("is_loaded").toBool()) return;
    if (lbl->movie()) {
        lbl->movie()->stop();
        delete lbl->movie();
        lbl->setMovie(nullptr);
    }
    lbl->setPixmap(QPixmap());
    int w = lbl->property("saved_w").toInt();
    int h = lbl->property("saved_h").toInt();
    if (w > 0 && h > 0) lbl->setFixedSize(w, h);
    else lbl->setFixedSize(180, 100);

    QString path = lbl->property("file_path").toString();
    lbl->setText("📷 " + QFileInfo(path).fileName() + "\n(Scroll to view)");
    lbl->setObjectName("chat_msg_image");
    lbl->setProperty("is_placeholder", true);
    lbl->setProperty("is_loaded", false);
    lbl->style()->unpolish(lbl);
    lbl->style()->polish(lbl);
    runtime_chat_image_data.remove(path);
}

inline void chat_area_apply_image_data(chat_area_t &chat, QLabel *lbl, const QByteArray &data, int max_media_w, int max_media_h, bool scrolling_down = true) {
    if (!lbl || data.isEmpty()) return;
    QString clean_content = lbl->property("file_path").toString();
    QSize sz = calculate_media_display_size_from_data(data, max_media_w, max_media_h);
    lbl->setProperty("saved_w", sz.width());
    lbl->setProperty("saved_h", sz.height());
    lbl->setFixedSize(sz);
    lbl->setText("");
    lbl->setObjectName("chat_msg_image");
    lbl->setProperty("is_placeholder", false);
    lbl->setProperty("is_loaded", true);
    lbl->style()->unpolish(lbl);
    lbl->style()->polish(lbl);

    if (clean_content.toLower().endsWith(".gif")) {
        if (lbl->movie()) {
            lbl->movie()->stop();
            delete lbl->movie();
        }
        QBuffer *buf = new QBuffer(lbl);
        buf->setData(data);
        buf->open(QIODevice::ReadOnly);
        QMovie *movie = new QMovie(buf, QByteArray(), lbl);
        movie->setScaledSize(sz);
        lbl->setMovie(movie);
        movie->start();
    } else {
        QPixmap pix;
        if (pix.loadFromData(data)) {
            lbl->setPixmap(pix.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    lbl->setCursor(Qt::PointingHandCursor);
    lbl->installEventFilter(new ImageClickFilter(clean_content, lbl));

    QWidget *bubble = lbl->parentWidget();
    if (bubble) {
        int bubble_w = sz.width() + 12;
        bubble->setProperty("ideal_w", bubble_w);
        bubble->setFixedWidth(bubble_w);
    }
    lbl->setProperty("is_loaded", true);

    // Maintain sliding window: max 20 images loaded in memory
    chat.sliding_window_images.removeAll(lbl);
    if (scrolling_down) {
        chat.sliding_window_images.append(lbl);
        while (chat.sliding_window_images.size() > 20) {
            QPointer<QLabel> oldest = chat.sliding_window_images.takeFirst();
            if (oldest) chat_area_unload_image(chat, oldest.data());
        }
    } else {
        chat.sliding_window_images.prepend(lbl);
        while (chat.sliding_window_images.size() > 20) {
            QPointer<QLabel> oldest = chat.sliding_window_images.takeLast();
            if (oldest) chat_area_unload_image(chat, oldest.data());
        }
    }
}

inline void chat_area_update_sliding_window(chat_area_t &chat) {
    if (!chat.scroll_area || !chat.messages_container || !chat.scroll_area->viewport()) return;
    QScrollBar *vbar = chat.scroll_area->verticalScrollBar();
    if (!vbar) return;

    int cur_val = vbar->value();
    bool scrolling_down = (cur_val >= chat.last_scroll_val);
    chat.last_scroll_val = cur_val;

    int vp_h = chat.scroll_area->viewport()->height();
    int vp_top = cur_val;
    int vp_bottom = cur_val + vp_h;

    int vp_w = chat.scroll_area->viewport()->width();
    int max_media_w = qBound(160, (int)(vp_w * 0.30), 300);
    int max_media_h = qBound(160, (int)(vp_w * 0.40), 300);

    for (QLabel *lbl : chat.messages_container->findChildren<QLabel*>()) {
        QString path = lbl->property("file_path").toString();
        if (path.isEmpty() || lbl->property("is_video").toBool()) continue;

        QPoint p = lbl->mapTo(chat.messages_container, QPoint(0, 0));
        int y = p.y();
        int h = lbl->height();
        bool in_window = (y + h >= vp_top - 300 && y <= vp_bottom + 300);
        if (in_window) {
            if (!lbl->property("is_loaded").toBool()) {
                if (runtime_chat_image_data.contains(path)) {
                    chat_area_apply_image_data(chat, lbl, runtime_chat_image_data[path], max_media_w, max_media_h, scrolling_down);
                } else if (!pending_chat_image_downloads.contains(path)) {
                    lbl->setText("📷 " + QFileInfo(path).fileName() + "\n(Loading...)");
                    lbl->setObjectName("chat_msg_image");
                    lbl->setProperty("is_placeholder", true);
                    lbl->style()->unpolish(lbl);
                    lbl->style()->polish(lbl);
                    user_request_file_download(chat.current_room_id(), path, 0);
                }
            }
        }
    }
}

inline QWidget* create_chat_bubble(chat_area_t &chat, uint64_t msg_id, bool is_my_message, const QString &content, uint16_t msg_type) {
    int vp_w = (chat.scroll_area && chat.scroll_area->viewport()) ? chat.scroll_area->viewport()->width() : 300;
    int max_w = qMax(180, (int)(vp_w * 0.70));
    int max_media_w = qBound(160, (int)(vp_w * 0.30), 300);
    int max_media_h = qBound(160, (int)(vp_w * 0.40), 300);

    QWidget *bubble_widget = new QWidget();
    bubble_widget->setObjectName(is_my_message ? "chat_msg_bubble_me" : "chat_msg_bubble_other");
    bubble_widget->setAttribute(Qt::WA_StyledBackground, true);
    bubble_widget->setMaximumWidth(max_w);
    bubble_widget->setProperty("msg_id", (qulonglong)msg_id);

    QVBoxLayout *bubble_layout = new QVBoxLayout(bubble_widget);
    bubble_layout->setContentsMargins(6, 6, 6, 6);
    bubble_layout->setSpacing(0);

    int ideal_w = 0;
    QString clean_content = content;
    clean_content.replace("\\", "/");

    if (msg_type == MESSAGE_TYPE::TEXT) {
        QString html_content = render_markdown_to_html(content);
        QLabel *content_lbl = new QLabel(bubble_widget);
        content_lbl->setObjectName("chat_msg_content");
        content_lbl->setProperty("raw_content", content);
        content_lbl->setText(html_content);
        content_lbl->setWordWrap(true);
        content_lbl->setOpenExternalLinks(true);
        content_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard);

        QFont font = content_lbl->font();
        font.setPixelSize(13);
        content_lbl->setFont(font);

        QTextDocument measure_doc;
        measure_doc.setDefaultFont(font);
        measure_doc.setDocumentMargin(0);
        measure_doc.setHtml(html_content);
        int text_ideal_w = qMin(max_w, qCeil(measure_doc.idealWidth()) + 12);

        content_lbl->setMaximumWidth(max_w);
        bubble_widget->setProperty("text_ideal_w", text_ideal_w);
        ideal_w = text_ideal_w;
        bubble_layout->addWidget(content_lbl);
    } else if (msg_type == MESSAGE_TYPE::IMAGE) {
        QLabel *img_lbl = new QLabel(bubble_widget);
        img_lbl->setObjectName("chat_msg_image");
        img_lbl->setProperty("file_path", clean_content);
        img_lbl->setProperty("is_loaded", false);
        img_lbl->setProperty("is_placeholder", true);
        img_lbl->setAlignment(Qt::AlignCenter);

        if (runtime_chat_image_data.contains(clean_content)) {
            chat_area_apply_image_data(chat, img_lbl, runtime_chat_image_data[clean_content], max_media_w, max_media_h, true);
            ideal_w = img_lbl->width() + 12;
        } else {
            img_lbl->setText("📷 " + QFileInfo(clean_content).fileName() + "\n(Loading...)");
            img_lbl->setFixedSize(180, 100);
            ideal_w = 192;
            user_request_file_download(chat.current_room_id(), clean_content, 0);
        }
        bubble_layout->addWidget(img_lbl);
    } else if (msg_type == MESSAGE_TYPE::VIDEO) {
        QWidget *video_card = new QWidget(bubble_widget);
        video_card->setObjectName("chat_file_card");
        video_card->setProperty("file_path", clean_content);
        video_card->setProperty("is_video", true);

        QVBoxLayout *card_layout = new QVBoxLayout(video_card);
        card_layout->setContentsMargins(4, 2, 4, 2);
        card_layout->setSpacing(6);

        QWidget *top_row = new QWidget(video_card);
        QHBoxLayout *top_layout = new QHBoxLayout(top_row);
        top_layout->setContentsMargins(0, 0, 0, 0);
        top_layout->setSpacing(6);

        QLabel *v_icon = new QLabel("🎬", top_row);
        v_icon->setObjectName("chat_file_type_icon");

        QLabel *v_name = new QLabel(QFileInfo(clean_content).fileName(), top_row);
        v_name->setObjectName("chat_file_name");
        v_name->setTextInteractionFlags(Qt::TextSelectableByMouse);

        top_layout->addWidget(v_icon);
        top_layout->addWidget(v_name, 1);
        card_layout->addWidget(top_row);

        QWidget *btn_row = new QWidget(video_card);
        btn_row->setObjectName("chat_file_btn_row");
        QHBoxLayout *btn_layout = new QHBoxLayout(btn_row);
        btn_layout->setContentsMargins(0, 0, 0, 0);
        btn_layout->setSpacing(6);
        card_layout->addWidget(btn_row);

        QString cached = get_cached_file_path(clean_content);
        setup_file_card_actions(video_card, clean_content, cached, true, chat.current_room_id());

        bubble_layout->addWidget(video_card);
        ideal_w = qMax(240, qMin(max_w, 300));
    } else { // OTHER / File
        QWidget *file_card = new QWidget(bubble_widget);
        file_card->setObjectName("chat_file_card");
        file_card->setProperty("file_path", clean_content);
        file_card->setProperty("is_video", false);

        QVBoxLayout *card_layout = new QVBoxLayout(file_card);
        card_layout->setContentsMargins(4, 2, 4, 2);
        card_layout->setSpacing(6);

        QWidget *top_row = new QWidget(file_card);
        QHBoxLayout *top_layout = new QHBoxLayout(top_row);
        top_layout->setContentsMargins(0, 0, 0, 0);
        top_layout->setSpacing(6);

        QLabel *f_icon = new QLabel("📄", file_card);
        f_icon->setObjectName("chat_file_type_icon");

        QLabel *f_name = new QLabel(QFileInfo(clean_content).fileName(), file_card);
        f_name->setObjectName("chat_file_name");
        f_name->setTextInteractionFlags(Qt::TextSelectableByMouse);

        top_layout->addWidget(f_icon);
        top_layout->addWidget(f_name, 1);
        card_layout->addWidget(top_row);

        QWidget *btn_row = new QWidget(file_card);
        btn_row->setObjectName("chat_file_btn_row");
        QHBoxLayout *btn_layout = new QHBoxLayout(btn_row);
        btn_layout->setContentsMargins(0, 0, 0, 0);
        btn_layout->setSpacing(6);
        card_layout->addWidget(btn_row);

        QString cached = get_cached_file_path(clean_content);
        setup_file_card_actions(file_card, clean_content, cached, false, chat.current_room_id());

        bubble_layout->addWidget(file_card);
        ideal_w = qMax(240, qMin(max_w, 300));
    }

    bubble_widget->setProperty("ideal_w", ideal_w);
    bubble_widget->setFixedWidth(qMin(max_w, ideal_w));

    uint64_t r_id = chat.current_room_id();
    auto show_context_menu = [bubble_widget, r_id, msg_id, is_my_message, clean_content, msg_type](const QPoint &global_pos) {
        QMenu menu(bubble_widget);
        menu.setObjectName("chat_context_menu");

        if (msg_type == MESSAGE_TYPE::TEXT) {
            QAction *copy_act = menu.addAction("Copy");
            QObject::connect(copy_act, &QAction::triggered, [bubble_widget, clean_content]() {
                QLabel *lbl = bubble_widget->findChild<QLabel*>("chat_msg_content");
                if (lbl && lbl->hasSelectedText()) {
                    QApplication::clipboard()->setText(lbl->selectedText());
                } else {
                    QApplication::clipboard()->setText(clean_content);
                }
            });
        } else if (msg_type == MESSAGE_TYPE::IMAGE) {
            QAction *copy_act = menu.addAction("Copy Image");
            QObject::connect(copy_act, &QAction::triggered, [clean_content]() {
                QString cached = get_cached_file_path(clean_content);
                if (!cached.isEmpty() && QFile::exists(cached)) {
                    QImage img(cached);
                    if (!img.isNull()) {
                        QApplication::clipboard()->setImage(img);
                    }
                }
            });
        } else {
            QAction *copy_act = menu.addAction("Copy File Path");
            QObject::connect(copy_act, &QAction::triggered, [clean_content]() {
                QString cached = get_cached_file_path(clean_content);
                QApplication::clipboard()->setText(cached.isEmpty() ? clean_content : cached);
            });
        }

        if (is_my_message && msg_id > 0) {
            menu.addSeparator();
            QAction *del_act = menu.addAction("Delete Message");
            QObject::connect(del_act, &QAction::triggered, [r_id, msg_id]() {
                user_delete_message(r_id, msg_id);
            });
        }

        menu.exec(global_pos);
    };

    bubble_widget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(bubble_widget, &QWidget::customContextMenuRequested, [bubble_widget, show_context_menu](const QPoint &pos) {
        show_context_menu(bubble_widget->mapToGlobal(pos));
    });

    QLabel *c_lbl = bubble_widget->findChild<QLabel*>("chat_msg_content");
    if (c_lbl) {
        c_lbl->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(c_lbl, &QWidget::customContextMenuRequested, [c_lbl, show_context_menu](const QPoint &pos) {
            show_context_menu(c_lbl->mapToGlobal(pos));
        });
    }

    QLabel *i_lbl = bubble_widget->findChild<QLabel*>("chat_msg_image");
    if (i_lbl) {
        i_lbl->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(i_lbl, &QWidget::customContextMenuRequested, [i_lbl, show_context_menu](const QPoint &pos) {
            show_context_menu(i_lbl->mapToGlobal(pos));
        });
    }

    return bubble_widget;
}

inline QWidget* create_chat_bubble_row(QWidget *bubble_widget, const QString &time_str, bool is_my_message) {
    QWidget *row = new QWidget();
    row->setObjectName("chat_bubble_row");
    QHBoxLayout *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    QLabel *time_lbl = new QLabel(time_str, row);
    time_lbl->setObjectName("chat_msg_time");

    if (is_my_message) {
        lay->addStretch(1);
        lay->addWidget(time_lbl, 0, Qt::AlignBottom);
        lay->addWidget(bubble_widget, 0, Qt::AlignBottom);
    } else {
        lay->addWidget(bubble_widget, 0, Qt::AlignBottom);
        lay->addWidget(time_lbl, 0, Qt::AlignBottom);
        lay->addStretch(1);
    }

    return row;
}

inline QWidget* create_chat_group_widget(chat_area_t &chat, uint64_t sender_id, const QString &sender, const QString &sender_avatar, const QString &date_key, bool is_my_message, QVBoxLayout **out_bubbles_layout) {
    QWidget *group_widget = new QWidget();
    group_widget->setObjectName("chat_group_widget");
    group_widget->setProperty("sender_id", (qulonglong)sender_id);
    group_widget->setProperty("date_key", date_key);

    QHBoxLayout *group_layout = new QHBoxLayout(group_widget);
    group_layout->setContentsMargins(0, 3, 0, 3);
    group_layout->setSpacing(8);

    // Avatar (always static, cover-cropped)
    QLabel *avt_label = new QLabel(group_widget);
    avt_label->setObjectName("chat_msg_avatar");
    avt_label->setFixedSize(40, 40);
    avt_label->setAlignment(Qt::AlignCenter);
    avt_label->setContextMenuPolicy(Qt::CustomContextMenu);
    avt_label->setCursor(Qt::PointingHandCursor);
    uint64_t target_user_id = sender_id;
    QString target_uname = sender;
    QString avt_to_use = is_my_message ? (sender_avatar.isEmpty() ? client.user_avatar : sender_avatar) : sender_avatar;

    auto show_user_menu = [chat, target_user_id, target_uname, avt_to_use](QWidget *anchor, const QPoint &pos) {
        if (target_user_id == 0) return;
        QMenu menu(anchor);
        menu.setObjectName("chat_context_menu");
        QAction *profile_act = menu.addAction(QString("View Profile: %1").arg(target_uname));
        QObject::connect(profile_act, &QAction::triggered, [chat, target_user_id, target_uname, avt_to_use]() {
            open_user_info_popup(chat.parent, target_user_id, target_uname, avt_to_use);
        });

        if (target_user_id != client.user_id) {
            QAction *poke_act = menu.addAction(QString("Poke %1").arg(target_uname));
            QObject::connect(poke_act, &QAction::triggered, [target_user_id]() {
                user_poke_member(target_user_id);
            });
        }

        if (client.is_admin) {
            QAction *roles_act = menu.addAction(QString("Roles: %1").arg(target_uname));
            QObject::connect(roles_act, &QAction::triggered, [chat, target_user_id, target_uname]() {
                open_user_roles_popup(chat.parent, target_user_id, target_uname);
            });
        }
        menu.exec(anchor->mapToGlobal(pos));
    };

    QObject::connect(avt_label, &QWidget::customContextMenuRequested, [avt_label, show_user_menu](const QPoint &pos) {
        show_user_menu(avt_label, pos);
    });

    class ChatAvtClickFilter : public QObject {
    public:
        QString avt;
        QString name;
        explicit ChatAvtClickFilter(const QString &a, const QString &n, QObject *parent = nullptr)
            : QObject(parent), avt(a), name(n) {}
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    view_avatar_full(avt, name);
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };
    avt_label->installEventFilter(new ChatAvtClickFilter(avt_to_use, target_uname, avt_label));
    avt_label->setToolTip(QString("Click to view %1's avatar").arg(target_uname));

    QString default_avt_path = QCoreApplication::applicationDirPath() + "/assets/defaults/user.png";
    QPixmap def_p(default_avt_path);
    avt_label->setPixmap(crop_avatar_cover(def_p, QSize(40, 40), 20));

    avt_label->setProperty("sender_avatar", avt_to_use);

    if (!avt_to_use.isEmpty()) {
        if (runtime_avatar_pixmaps.contains(avt_to_use)) {
            QPixmap p = runtime_avatar_pixmaps.value(avt_to_use);
            if (!p.isNull()) {
                avt_label->setPixmap(crop_avatar_cover(p, QSize(40, 40), 20));
            }
        } else {
            user_request_file_download(0, avt_to_use, 0);
        }
    }

    QWidget *col_widget = new QWidget(group_widget);
    QVBoxLayout *col_layout = new QVBoxLayout(col_widget);
    col_layout->setContentsMargins(0, 0, 0, 0);
    col_layout->setSpacing(3);

    // Meta row (username only for other users, above the bubbles)
    if (!is_my_message) {
        QHBoxLayout *meta_layout = new QHBoxLayout();
        meta_layout->setContentsMargins(2, 0, 2, 0);
        meta_layout->setSpacing(6);
        QLabel *sender_lbl = new QLabel(sender, col_widget);
        sender_lbl->setObjectName("chat_msg_sender");
        sender_lbl->setContextMenuPolicy(Qt::CustomContextMenu);
        sender_lbl->setCursor(Qt::PointingHandCursor);
        QObject::connect(sender_lbl, &QWidget::customContextMenuRequested, [sender_lbl, show_user_menu](const QPoint &pos) {
            show_user_menu(sender_lbl, pos);
        });
        meta_layout->addWidget(sender_lbl);
        meta_layout->addStretch(1);
        col_layout->addLayout(meta_layout);
    }

    QVBoxLayout *bubbles_layout = new QVBoxLayout();
    bubbles_layout->setContentsMargins(0, 0, 0, 0);
    bubbles_layout->setSpacing(4);
    col_layout->addLayout(bubbles_layout);

    if (out_bubbles_layout) {
        *out_bubbles_layout = bubbles_layout;
    }

    if (is_my_message) {
        group_layout->addStretch(1);
        group_layout->addWidget(col_widget);
        group_layout->addWidget(avt_label, 0, Qt::AlignTop);
    } else {
        group_layout->addWidget(avt_label, 0, Qt::AlignTop);
        group_layout->addWidget(col_widget);
        group_layout->addStretch(1);
    }

    return group_widget;
}

inline void chat_area_add_message(chat_area_t &chat, uint64_t sender_id, const QString &sender, const QString &sender_avatar, const QString &content, uint16_t msg_type = MESSAGE_TYPE::TEXT, const QString &time_str = "", uint64_t msg_id = 0, const QDateTime &timestamp = QDateTime()) {
    if (!chat.messages_layout) return;

    bool is_my_message = (sender_id > 0 && sender_id == client.user_id);
    if (is_my_message && chat.widget) {
        chat.widget->setProperty("auto_scroll_down", true);
    }

    QDateTime msg_time = timestamp.isValid() ? timestamp : QDateTime::currentDateTime();
    QString formatted_time = time_str.isEmpty() ? msg_time.toString("HH:mm:ss") : time_str;

    if (chat.oldest_msg_id == 0 || (msg_id > 0 && msg_id < chat.oldest_msg_id)) {
        chat.oldest_msg_id = msg_id;
    }
    if (msg_id > chat.newest_msg_id) {
        chat.newest_msg_id = msg_id;
    }

    // Date separator
    QString date_key = msg_time.toString("yyyy-MM-dd");
    if (date_key != chat.last_date_key) {
        QWidget *sep = create_chat_date_separator(msg_time.date());
        chat.messages_layout->addWidget(sep);
        chat.last_date_key = date_key;
        chat.last_group_widget = nullptr;
        chat.last_bubbles_layout = nullptr;
        chat.last_group_sender_id = (uint64_t)-1;
    }

    // Bubble and row
    QWidget *bubble = create_chat_bubble(chat, msg_id, is_my_message, content, msg_type);
    QWidget *bubble_row = create_chat_bubble_row(bubble, formatted_time, is_my_message);

    // Grouping
    if (sender_id == chat.last_group_sender_id && chat.last_bubbles_layout != nullptr) {
        chat.last_bubbles_layout->addWidget(bubble_row);
    } else {
        QVBoxLayout *b_lay = nullptr;
        QWidget *group = create_chat_group_widget(chat, sender_id, sender, sender_avatar, date_key, is_my_message, &b_lay);
        b_lay->addWidget(bubble_row);
        chat.messages_layout->addWidget(group);
        chat.last_group_widget = group;
        chat.last_bubbles_layout = b_lay;
        chat.last_group_sender_id = sender_id;
    }

    QScrollBar *bar = chat.scroll_area ? chat.scroll_area->verticalScrollBar() : nullptr;
    bool was_auto_scroll = chat.widget ? chat.widget->property("auto_scroll_down").toBool() : true;
    if (was_auto_scroll && bar) {
        if (chat.new_messages_bar) chat.new_messages_bar->hide();
        QTimer::singleShot(10, [bar]() {
            bar->setValue(bar->maximum());
        });
    } else {
        if (chat.new_messages_bar) chat.new_messages_bar->show();
    }
    chat_area_update_sliding_window(chat);
}

inline bool chat_area_enqueue_message(chat_area_t &chat, const chat_message_t &msg) {
    if (msg.room_id != chat.current_room_id()) return false;

    bool is_history_prepend = (chat.oldest_msg_id > 0 && msg.id > 0 && msg.id < chat.oldest_msg_id);
    if (chat.is_loading_history && (is_history_prepend || chat.oldest_msg_id == 0)) {
        chat.pending_history_batch.append(msg);
        return false;
    }

    if (chat.newest_msg_id > 0 && msg.id > 0 && msg.id <= chat.newest_msg_id) {
        return false;
    }

    QString time_str = msg.timestamp.toString("HH:mm:ss");
    chat_area_add_message(chat, msg.sender_id, msg.sender, msg.sender_avatar, msg.msg,
                          msg.msg_type, time_str, msg.id, msg.timestamp);
    return true;
}

inline void chat_area_check_fill_viewport(chat_area_t &chat) {
    if (!chat.widget || !chat.scroll_area || !chat.messages_container ||
        !chat.has_more_history || chat.is_loading_history ||
        chat.oldest_msg_id == 0 || chat.current_room_id() == 0) {
        return;
    }

    QPointer<QWidget> chat_widget_ptr(chat.widget);
    uint64_t expected_room_id = chat.current_room_id();
    uint64_t oldest_id = chat.oldest_msg_id;

    QTimer::singleShot(60, [chat_widget_ptr, expected_room_id, oldest_id]() {
        if (!chat_widget_ptr) return;
        chat_area_t *c = (chat_area_t*)chat_widget_ptr->property("chat_area_ptr").value<void*>();
        if (!c || c->current_room_id() != expected_room_id || !c->has_more_history ||
            c->is_loading_history || c->oldest_msg_id != oldest_id) {
            return;
        }

        QScrollBar *bar = c->scroll_area ? c->scroll_area->verticalScrollBar() : nullptr;
        QWidget *viewport = c->scroll_area ? c->scroll_area->viewport() : nullptr;
        if (!bar || !viewport || !c->messages_container) return;

        c->messages_container->layout()->activate();
        c->messages_container->adjustSize();

        if (bar->maximum() <= 10 || c->messages_container->height() <= viewport->height()) {
            c->is_loading_history = true;
            user_request_history(expected_room_id, oldest_id, 20);
        }
    });
}

inline void chat_area_flush_history_batch(chat_area_t &chat) {
    chat.is_loading_history = false;
    if (chat.pending_history_batch.isEmpty() || !chat.messages_layout) return;

    QList<chat_message_t> batch = chat.pending_history_batch;
    chat.pending_history_batch.clear();

    QScrollBar *bar = chat.scroll_area ? chat.scroll_area->verticalScrollBar() : nullptr;

    // Initial load: append all messages and scroll to bottom
    if (chat.messages_layout->count() == 0) {
        for (const chat_message_t &m : batch) {
            chat_area_add_message(chat, m.sender_id, m.sender, m.sender_avatar, m.msg,
                                  m.msg_type, m.timestamp.toString("HH:mm:ss"), m.id, m.timestamp);
        }
        if (bar && chat.messages_container) {
            chat.messages_container->layout()->activate();
            chat.messages_container->adjustSize();
            bar->setValue(bar->maximum());
            QTimer::singleShot(20, [bar]() {
                bar->setValue(bar->maximum());
            });
        }
        chat_area_check_fill_viewport(chat);
        return;
    }

    // Prepending older messages when scrolled up:
    // Identify top-most visible widget currently in viewport as our anchor
    QWidget *anchor_widget = nullptr;
    int anchor_y_viewport = 0;
    if (chat.scroll_area && chat.scroll_area->viewport()) {
        QWidget *viewport = chat.scroll_area->viewport();
        for (int i = 0; i < chat.messages_layout->count(); ++i) {
            QLayoutItem *item = chat.messages_layout->itemAt(i);
            if (!item) continue;
            QWidget *w = item->widget();
            if (w && w->isVisible()) {
                QPoint pt = w->mapTo(viewport, QPoint(0, 0));
                if (pt.y() >= -w->height() && pt.y() <= viewport->height()) {
                    anchor_widget = w;
                    anchor_y_viewport = pt.y();
                    break;
                }
            }
        }
        if (!anchor_widget && chat.messages_layout->count() > 0) {
            QLayoutItem *first = chat.messages_layout->itemAt(0);
            if (first && first->widget()) {
                anchor_widget = first->widget();
                anchor_y_viewport = anchor_widget->mapTo(viewport, QPoint(0, 0)).y();
            }
        }
    }

    QList<QWidget*> prepended_widgets;
    QString hist_date_key;
    QWidget *hist_group = nullptr;
    QVBoxLayout *hist_bubbles_lay = nullptr;
    uint64_t hist_sender_id = (uint64_t)-1;

    for (const chat_message_t &m : batch) {
        if (chat.oldest_msg_id == 0 || m.id < chat.oldest_msg_id) {
            chat.oldest_msg_id = m.id;
        }
        QString date_key = m.timestamp.toString("yyyy-MM-dd");
        if (date_key != hist_date_key) {
            QWidget *sep = create_chat_date_separator(m.timestamp.date());
            prepended_widgets.append(sep);
            hist_date_key = date_key;
            hist_group = nullptr;
            hist_bubbles_lay = nullptr;
            hist_sender_id = (uint64_t)-1;
        }

        bool is_mine = (m.sender_id > 0 && m.sender_id == client.user_id);
        QWidget *bubble = create_chat_bubble(chat, m.id, is_mine, m.msg, m.msg_type);
        QWidget *bubble_row = create_chat_bubble_row(bubble, m.timestamp.toString("HH:mm:ss"), is_mine);

        if (m.sender_id == hist_sender_id && hist_bubbles_lay != nullptr) {
            hist_bubbles_lay->addWidget(bubble_row);
        } else {
            hist_group = create_chat_group_widget(chat, m.sender_id, m.sender, m.sender_avatar, date_key, is_mine, &hist_bubbles_lay);
            hist_bubbles_lay->addWidget(bubble_row);
            prepended_widgets.append(hist_group);
            hist_sender_id = m.sender_id;
        }
    }

    for (int i = 0; i < prepended_widgets.size(); ++i) {
        chat.messages_layout->insertWidget(i, prepended_widgets[i]);
    }

    if (anchor_widget && bar && chat.messages_container) {
        chat.messages_container->layout()->activate();
        chat.messages_container->adjustSize();

        QPoint new_pt = anchor_widget->mapTo(chat.messages_container, QPoint(0, 0));
        int target_val = new_pt.y() - anchor_y_viewport;
        bar->setValue(target_val);

        QPointer<QWidget> anchor_ptr(anchor_widget);
        QPointer<QWidget> container_ptr(chat.messages_container);
        QPointer<QScrollBar> bar_ptr(bar);
        QTimer::singleShot(0, [bar_ptr, anchor_ptr, container_ptr, anchor_y_viewport]() {
            if (bar_ptr && anchor_ptr && container_ptr) {
                container_ptr->layout()->activate();
                container_ptr->adjustSize();
                QPoint p = anchor_ptr->mapTo(container_ptr, QPoint(0, 0));
                bar_ptr->setValue(p.y() - anchor_y_viewport);
            }
        });
    }

    chat_area_check_fill_viewport(chat);
    chat_area_update_sliding_window(chat);
}

inline void chat_area_handle_file_downloaded(chat_area_t &chat, const QString &file_path, const QString &local_abs_path) {
    if (!chat.messages_container) return;
    int vp_w = chat.scroll_area && chat.scroll_area->viewport() ? chat.scroll_area->viewport()->width() : 300;
    int max_media_w = qBound(160, (int)(vp_w * 0.3), 300);
    int max_media_h = qBound(160, (int)(vp_w * 0.4), 300);

    QString clean = file_path;
    clean.replace("\\", "/");

    if (runtime_chat_image_data.contains(clean)) {
        for (QLabel *lbl : chat.messages_container->findChildren<QLabel*>()) {
            if (lbl->property("file_path").toString() == clean) {
                int old_h = lbl->height();
                int old_y = lbl->mapTo(chat.messages_container, QPoint(0, 0)).y();
                chat_area_apply_image_data(chat, lbl, runtime_chat_image_data[clean], max_media_w, max_media_h, true);

                QScrollBar *bar = chat.scroll_area ? chat.scroll_area->verticalScrollBar() : nullptr;
                int diff_h = lbl->height() - old_h;
                if (bar && chat.widget && !chat.widget->property("auto_scroll_down").toBool() && old_y < bar->value() && diff_h != 0) {
                    bar->setValue(bar->value() + diff_h);
                }
            }
        }
    } else if (!local_abs_path.isEmpty() && QFile::exists(local_abs_path)) {
        for (QLabel *lbl : chat.messages_container->findChildren<QLabel*>()) {
            if (lbl->property("file_path").toString() == clean) {
                int old_h = lbl->height();
                int old_y = lbl->mapTo(chat.messages_container, QPoint(0, 0)).y();
                lbl->setText("");
                lbl->setObjectName("chat_msg_image");
                lbl->setProperty("is_placeholder", false);
                lbl->setProperty("is_loaded", true);
                lbl->style()->unpolish(lbl);
                lbl->style()->polish(lbl);
                QSize sz = calculate_media_display_size(local_abs_path, max_media_w, max_media_h);
                lbl->setFixedSize(sz);
                if (clean.toLower().endsWith(".gif")) {
                    if (lbl->movie()) {
                        lbl->movie()->stop();
                        delete lbl->movie();
                    }
                    QMovie *movie = new QMovie(local_abs_path, QByteArray(), lbl);
                    movie->setScaledSize(sz);
                    lbl->setMovie(movie);
                    movie->start();
                } else {
                    QPixmap pix(local_abs_path);
                    if (!pix.isNull()) {
                        lbl->setPixmap(pix.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
                lbl->setCursor(Qt::PointingHandCursor);
                lbl->installEventFilter(new ImageClickFilter(local_abs_path, lbl));

                QWidget *bubble = lbl->parentWidget();
                if (bubble) {
                    int bubble_w = sz.width() + 12;
                    bubble->setProperty("ideal_w", bubble_w);
                    bubble->setFixedWidth(bubble_w);
                }

                QScrollBar *bar = chat.scroll_area ? chat.scroll_area->verticalScrollBar() : nullptr;
                int diff_h = sz.height() - old_h;
                if (bar && chat.widget && !chat.widget->property("auto_scroll_down").toBool() && old_y < bar->value() && diff_h != 0) {
                    bar->setValue(bar->value() + diff_h);
                }
            }
        }
    }

    for (QWidget *card : chat.messages_container->findChildren<QWidget*>("chat_file_card")) {
        if (card->property("file_path").toString() == clean) {
            bool is_video = card->property("is_video").toBool();
            setup_file_card_actions(card, clean, local_abs_path, is_video, chat.current_room_id());
        }
    }

    QScrollBar *bar = chat.scroll_area ? chat.scroll_area->verticalScrollBar() : nullptr;
    if (bar && chat.widget && chat.widget->property("auto_scroll_down").toBool()) {
        QTimer::singleShot(20, [bar]() {
            bar->setValue(bar->maximum());
        });
    }
}

inline void chat_area_remove_message(chat_area_t &chat, uint64_t msg_id) {
    if (!chat.messages_container || msg_id == 0) return;
    for (QWidget *w : chat.messages_container->findChildren<QWidget*>()) {
        if (w->property("msg_id").toULongLong() == msg_id) {
            QWidget *bubble_row = w->parentWidget();
            if (bubble_row && bubble_row->objectName() == "chat_bubble_row") {
                QWidget *bubbles_col = bubble_row->parentWidget();
                bubble_row->deleteLater();
                if (bubbles_col) {
                    QTimer::singleShot(50, [bubbles_col]() {
                        if (bubbles_col->findChildren<QWidget*>("chat_bubble_row").isEmpty()) {
                            QWidget *group = bubbles_col->parentWidget();
                            if (group && group->objectName() == "chat_group_widget") {
                                group->deleteLater();
                            }
                        }
                    });
                }
            } else {
                w->deleteLater();
            }
            break;
        }
    }
}

inline void chat_area_update_avatar(chat_area_t &chat, const QString &avatar_path) {
    if (!chat.messages_container || !runtime_avatar_pixmaps.contains(avatar_path)) return;
    QPixmap p = runtime_avatar_pixmaps.value(avatar_path);
    if (p.isNull()) return;
    QPixmap cropped = crop_avatar_cover(p, QSize(40, 40), 20);
    for (QLabel *lbl : chat.messages_container->findChildren<QLabel*>("chat_msg_avatar")) {
        if (lbl->property("sender_avatar").toString() == avatar_path) {
            lbl->setPixmap(cropped);
        }
    }
}

inline void chat_area_update_media_sizes(chat_area_t &chat) {
    if (chat.scroll_area && chat.messages_container) {
        chat_area_update_media_sizes(chat.scroll_area, chat.messages_container);
    }
}

inline void chat_area_update_theme_icons(chat_area_t &chat) {
    QString icons_dir = QCoreApplication::applicationDirPath() + "/assets/icons/";
    QColor fg(global_theme.foreground);
    if (chat.close_btn) {
        chat.close_btn->setIcon(QIcon(colored_svg(icons_dir + "close.svg", fg, QSize(12, 12))));
    }
    if (chat.pending_clear_btn) {
        chat.pending_clear_btn->setIcon(QIcon(colored_svg(icons_dir + "close.svg", fg, QSize(12, 12))));
    }
    if (chat.bold_btn) {
        chat.bold_btn->setIcon(QIcon(colored_svg(icons_dir + "bold.svg", fg, QSize(14, 14))));
    }
    if (chat.italic_btn) {
        chat.italic_btn->setIcon(QIcon(colored_svg(icons_dir + "italic.svg", fg, QSize(14, 14))));
    }
    if (chat.underline_btn) {
        chat.underline_btn->setIcon(QIcon(colored_svg(icons_dir + "underline.svg", fg, QSize(14, 14))));
    }
    if (chat.file_btn) {
        chat.file_btn->setIcon(QIcon(colored_svg(icons_dir + "file.svg", fg, QSize(14, 14))));
    }
    if (chat.emoji_btn) {
        chat.emoji_btn->setIcon(QIcon(colored_svg(icons_dir + "emoji.svg", fg, QSize(14, 14))));
    }
    if (chat.messages_container) {
        for (QLabel *lbl : chat.messages_container->findChildren<QLabel*>("chat_msg_content")) {
            QString raw = lbl->property("raw_content").toString();
            if (!raw.isEmpty()) {
                lbl->setText(render_markdown_to_html(raw));
            }
        }
    }
}

inline void chat_area_open_room(chat_area_t &chat, uint64_t room_id, const QString &room_name) {
    uint64_t prev_id = chat.current_room_id();
    if (prev_id > 0 && prev_id != room_id) {
        user_leave_room(prev_id, 0);
    }

    chat.set_room_id(room_id);
    chat.current_room_name = room_name;

    // Clear messages container
    if (chat.messages_layout) {
        while (QLayoutItem *item = chat.messages_layout->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    chat.oldest_msg_id = 0;
    chat.newest_msg_id = 0;
    chat.has_more_history = true;
    chat.is_loading_history = true;
    chat.pending_history_batch.clear();
    chat.sliding_window_images.clear();
    clear_pending_chat_downloads();
    chat.last_date_key.clear();
    chat.last_group_widget = nullptr;
    chat.last_bubbles_layout = nullptr;
    chat.last_group_sender_id = (uint64_t)-1;

    if (chat.title) {
        chat.title->setText("# " + room_name);
    }

    chat_area_clear_pending_file(chat);

    if (chat.preview_container) {
        chat.preview_container->hide();
    }

    if (chat.widget) {
        chat.widget->setProperty("auto_scroll_down", true);
        chat.widget->show();
    }

    if (chat.new_messages_bar) {
        chat.new_messages_bar->hide();
    }

    if (chat.input) {
        chat.input->clear();
        adjust_input_height(chat.input, 36, 130);
        chat.input->setFocus();
    }
}

inline chat_area_t create_chat_area(QWidget *parent) {
    chat_area_t chat;

    chat.parent = parent;
    chat.widget = new QWidget();
    chat.widget->setObjectName("chat_area");
    chat.widget->setAttribute(Qt::WA_StyledBackground, true);
    chat.widget->setAcceptDrops(true);
    chat.widget->setMinimumWidth(300);

    QVBoxLayout *main_layout = new QVBoxLayout(chat.widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 1. Header
    QWidget *header = new QWidget(chat.widget);
    header->setObjectName("chat_header");
    QHBoxLayout *header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(6, 6, 6, 6);
    header_layout->setSpacing(6);

    chat.title = new QLabel("# Room Chat", header);
    chat.title->setObjectName("chat_title");

    chat.close_btn = new QPushButton(header);
    chat.close_btn->setObjectName("chat_close_btn");
    chat.close_btn->setCursor(Qt::PointingHandCursor);
    chat.close_btn->setFixedSize(26, 26);
    chat.close_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/close.svg",
                                              QColor(global_theme.foreground), QSize(12, 12))));
    chat.close_btn->setToolTip("Close Chat");

    QWidget *chat_widget = chat.widget;
    chat_widget->setProperty("room_id", (qulonglong)0);
    chat_widget->setProperty("auto_scroll_down", true);
    QObject::connect(chat.close_btn, &QPushButton::clicked, [chat_widget]() {
        uint64_t cur_id = chat_widget->property("room_id").toULongLong();
        if (cur_id > 0) {
            user_leave_room(cur_id, 0);
            chat_widget->setProperty("room_id", (qulonglong)0);
        }
        chat_widget->hide();
    });

    header_layout->addWidget(chat.title, 1);
    header_layout->addWidget(chat.close_btn, 0);
    main_layout->addWidget(header);

    // 2. QScrollArea for chat messages
    chat.scroll_area = new QScrollArea(chat.widget);
    chat.scroll_area->setObjectName("chat_scroll_area");
    chat.scroll_area->setWidgetResizable(true);
    chat.scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chat.scroll_area->setFrameShape(QFrame::NoFrame);

    chat.messages_container = new QWidget();
    chat.messages_container->setObjectName("chat_messages_container");
    chat.messages_layout = new QVBoxLayout(chat.messages_container);
    chat.messages_layout->setContentsMargins(8, 8, 8, 8);
    chat.messages_layout->setSpacing(6);
    chat.messages_layout->setAlignment(Qt::AlignTop);

    chat.scroll_area->setWidget(chat.messages_container);
    main_layout->addWidget(chat.scroll_area, 1);

    chat.scroll_area->viewport()->installEventFilter(
        new ChatResizeFilter(chat.scroll_area, chat.messages_container, chat.scroll_area));
    chat.widget->installEventFilter(
        new ChatResizeFilter(chat.scroll_area, chat.messages_container, chat.widget));

    // New messages notification bar
    chat.new_messages_bar = new QPushButton("New messages ↓", chat.widget);
    chat.new_messages_bar->setObjectName("new_messages_bar");
    chat.new_messages_bar->setCursor(Qt::PointingHandCursor);
    chat.new_messages_bar->setFixedHeight(24);
    chat.new_messages_bar->hide();
    main_layout->addWidget(chat.new_messages_bar);

    QScrollBar *vbar = chat.scroll_area->verticalScrollBar();
    QPushButton *new_bar_ptr = chat.new_messages_bar;
    QScrollArea *sa_ptr = chat.scroll_area;
    QWidget *mc_ptr = chat.messages_container;

    if (vbar) {
        QObject::connect(vbar, &QScrollBar::rangeChanged, [vbar, chat_widget](int min, int max) {
            Q_UNUSED(min);
            if (chat_widget && chat_widget->property("auto_scroll_down").toBool()) {
                vbar->setValue(max);
            }
        });

        QObject::connect(vbar, &QScrollBar::valueChanged, [vbar, chat_widget, new_bar_ptr, sa_ptr, mc_ptr](int val) {
            if (!vbar || !chat_widget) return;
            update_active_chat_gifs(sa_ptr, mc_ptr);

            chat_area_t *chat_ptr = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
            if (chat_ptr) {
                chat_area_update_sliding_window(*chat_ptr);
            }

            bool at_bottom = (vbar->maximum() - val <= 10);
            chat_widget->setProperty("auto_scroll_down", at_bottom);
            if (at_bottom && new_bar_ptr && new_bar_ptr->isVisible()) {
                new_bar_ptr->hide();
            }

            if (val <= 10 && vbar->maximum() > 40) {
                if (chat_ptr && !chat_ptr->is_loading_history && chat_ptr->has_more_history && chat_ptr->oldest_msg_id > 0) {
                    chat_ptr->is_loading_history = true;
                    user_request_history(chat_ptr->current_room_id(), chat_ptr->oldest_msg_id, 20);
                }
            }
        });

        QObject::connect(chat.new_messages_bar, &QPushButton::clicked, [vbar, chat_widget, new_bar_ptr]() {
            if (chat_widget) chat_widget->setProperty("auto_scroll_down", true);
            if (vbar) vbar->setValue(vbar->maximum());
            if (new_bar_ptr) new_bar_ptr->hide();
        });
    }

    // 3. Input container
    QWidget *input_container = new QWidget(chat.widget);
    input_container->setObjectName("chat_input_container");
    QVBoxLayout *input_layout = new QVBoxLayout(input_container);
    input_layout->setContentsMargins(6, 4, 6, 6);
    input_layout->setSpacing(4);

    // Markdown preview container
    chat.preview_container = new QWidget(input_container);
    chat.preview_container->setObjectName("chat_preview_container");
    QVBoxLayout *preview_layout = new QVBoxLayout(chat.preview_container);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_layout->setSpacing(2);

    QLabel *preview_title = new QLabel("Markdown Preview", chat.preview_container);
    preview_title->setObjectName("chat_preview_title");
    preview_layout->addWidget(preview_title);

    QScrollArea *preview_scroll = new QScrollArea(chat.preview_container);
    preview_scroll->setObjectName("chat_preview_scroll");
    preview_scroll->setWidgetResizable(true);
    preview_scroll->setMaximumHeight(80);
    preview_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    chat.preview_content = new QLabel(preview_scroll);
    chat.preview_content->setObjectName("chat_preview_content");
    chat.preview_content->setWordWrap(true);
    chat.preview_content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    preview_scroll->setWidget(chat.preview_content);
    preview_layout->addWidget(preview_scroll);
    chat.preview_container->hide();
    input_layout->addWidget(chat.preview_container);

    // Pending file container
    chat.pending_container = new QWidget(input_container);
    chat.pending_container->setObjectName("chat_pending_bar");
    QHBoxLayout *pending_layout = new QHBoxLayout(chat.pending_container);
    pending_layout->setContentsMargins(6, 4, 6, 4);
    pending_layout->setSpacing(6);

    chat.pending_icon = new QLabel(chat.pending_container);
    chat.pending_icon->setFixedSize(32, 32);
    chat.pending_icon->setAlignment(Qt::AlignCenter);

    chat.pending_name = new QLabel(chat.pending_container);
    chat.pending_name->setObjectName("chat_pending_name");

    chat.pending_clear_btn = new QPushButton(chat.pending_container);
    chat.pending_clear_btn->setObjectName("chat_pending_clear_btn");
    chat.pending_clear_btn->setCursor(Qt::PointingHandCursor);
    chat.pending_clear_btn->setFixedSize(20, 20);
    chat.pending_clear_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/close.svg",
                                                      QColor(global_theme.foreground), QSize(10, 10))));
    chat.pending_clear_btn->setToolTip("Cancel Attachment");

    pending_layout->addWidget(chat.pending_icon, 0);
    pending_layout->addWidget(chat.pending_name, 1);
    pending_layout->addWidget(chat.pending_clear_btn, 0);
    chat.pending_container->hide();
    input_layout->addWidget(chat.pending_container);

    // Formatting Toolbar
    QWidget *toolbar = new QWidget(input_container);
    QHBoxLayout *toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(0, 0, 0, 0);
    toolbar_layout->setSpacing(4);

    chat.bold_btn = new QPushButton(toolbar);
    chat.bold_btn->setObjectName("chat_format_btn");
    chat.bold_btn->setCursor(Qt::PointingHandCursor);
    chat.bold_btn->setFixedSize(22, 22);
    chat.bold_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/bold.svg",
                                             QColor(global_theme.muted_foreground), QSize(14, 14))));
    chat.bold_btn->setToolTip("Bold (**text**)");

    chat.italic_btn = new QPushButton(toolbar);
    chat.italic_btn->setObjectName("chat_format_btn");
    chat.italic_btn->setCursor(Qt::PointingHandCursor);
    chat.italic_btn->setFixedSize(22, 22);
    chat.italic_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/italic.svg",
                                               QColor(global_theme.muted_foreground), QSize(14, 14))));
    chat.italic_btn->setToolTip("Italic (*text*)");

    chat.underline_btn = new QPushButton(toolbar);
    chat.underline_btn->setObjectName("chat_format_btn");
    chat.underline_btn->setCursor(Qt::PointingHandCursor);
    chat.underline_btn->setFixedSize(22, 22);
    chat.underline_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/underline.svg",
                                                  QColor(global_theme.muted_foreground), QSize(14, 14))));
    chat.underline_btn->setToolTip("Underline (<u>text</u>)");

    chat.file_btn = new QPushButton(toolbar);
    chat.file_btn->setObjectName("chat_format_btn");
    chat.file_btn->setCursor(Qt::PointingHandCursor);
    chat.file_btn->setFixedSize(22, 22);
    chat.file_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/file.svg",
                                             QColor(global_theme.muted_foreground), QSize(14, 14))));
    chat.file_btn->setToolTip("Attach File (Max 10MB)");

    chat.emoji_btn = new QPushButton(toolbar);
    chat.emoji_btn->setObjectName("chat_format_btn");
    chat.emoji_btn->setCursor(Qt::PointingHandCursor);
    chat.emoji_btn->setFixedSize(22, 22);
    chat.emoji_btn->setIcon(QIcon(colored_svg(QCoreApplication::applicationDirPath() + "/assets/icons/emoji.svg",
                                              QColor(global_theme.muted_foreground), QSize(14, 14))));
    chat.emoji_btn->setToolTip("Insert Emoji");

    toolbar_layout->addWidget(chat.bold_btn);
    toolbar_layout->addWidget(chat.italic_btn);
    toolbar_layout->addWidget(chat.underline_btn);
    toolbar_layout->addWidget(chat.file_btn);
    toolbar_layout->addWidget(chat.emoji_btn);
    toolbar_layout->addStretch(1);
    input_layout->addWidget(toolbar);

    // Auto-expanding QTextEdit input
    chat.input = new ChatTextEdit(input_container);
    chat.input->setObjectName("chat_input");
    chat.input->setPlaceholderText("Message in #" + chat.current_room_name + " (Enter to send, Shift+Enter for new line)");
    chat.input->setFixedHeight(36);
    input_layout->addWidget(chat.input);

    main_layout->addWidget(input_container);

    // Connections
    ChatTextEdit *input_ptr = chat.input;
    QPushButton *emoji_btn_ptr = chat.emoji_btn;
    QObject::connect(chat.bold_btn, &QPushButton::clicked, [input_ptr]() {
        chat_format_selection(input_ptr, "**", "**");
    });
    QObject::connect(chat.italic_btn, &QPushButton::clicked, [input_ptr]() {
        chat_format_selection(input_ptr, "*", "*");
    });
    QObject::connect(chat.underline_btn, &QPushButton::clicked, [input_ptr]() {
        chat_format_selection(input_ptr, "<u>", "</u>");
    });
    QObject::connect(chat.emoji_btn, &QPushButton::clicked, [chat_widget, emoji_btn_ptr, input_ptr]() {
        open_emoji_picker(chat_widget, emoji_btn_ptr, input_ptr);
    });

    QObject::connect(chat.file_btn, &QPushButton::clicked, [chat_widget]() {
        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (!c) return;
        QString path = QFileDialog::getOpenFileName(chat_widget->window(), "Select File Attachment", "",
                                                    "All Files (*.*);;Images (*.png *.jpg *.jpeg *.gif *.webp);;Videos (*.mp4 *.webm *.mkv)");
        if (!path.isEmpty()) {
            chat_area_set_pending_file(*c, path);
        }
    });

    QObject::connect(chat.pending_clear_btn, &QPushButton::clicked, [chat_widget]() {
        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (c) chat_area_clear_pending_file(*c);
    });

    // Markdown live preview
    QLabel *prev_content = chat.preview_content;
    QWidget *prev_container = chat.preview_container;
    QObject::connect(chat.input, &QTextEdit::textChanged, [input_ptr, prev_content, prev_container]() {
        adjust_input_height(input_ptr, 36, 130);
        QString raw = input_ptr->toPlainText();
        if (has_markdown_syntax(raw)) {
            QString html = render_markdown_to_html(raw);
            prev_content->setText(html);
            prev_container->show();
        } else {
            prev_container->hide();
        }
    });

    // Clipboard and Drag & Drop
    chat.input->on_mime_insert = [chat_widget](const QMimeData *mime) -> bool {
        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (!c) return false;

        if (mime->hasImage()) {
            QImage img = qvariant_cast<QImage>(mime->imageData());
            if (!img.isNull()) {
                QString temp_dir = QCoreApplication::applicationDirPath() + "/cache/temp";
                QDir().mkpath(temp_dir);
                QString temp_file = temp_dir + "/clipboard_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
                if (img.save(temp_file, "PNG")) {
                    chat_area_set_pending_file(*c, temp_file);
                    return true;
                }
            }
        }
        if (mime->hasUrls()) {
            QList<QUrl> urls = mime->urls();
            if (!urls.isEmpty()) {
                QString local_path = urls.first().toLocalFile();
                if (!local_path.isEmpty() && QFile::exists(local_path)) {
                    chat_area_set_pending_file(*c, local_path);
                    return true;
                }
            }
        }
        return false;
    };

    ChatKeyFilter *key_filter = new ChatKeyFilter(chat.input);
    key_filter->on_paste = [chat_widget]() -> bool {
        const QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mime = clipboard ? clipboard->mimeData() : nullptr;
        if (!mime) return false;

        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (!c) return false;

        if (mime->hasImage()) {
            QImage img = clipboard->image();
            if (!img.isNull()) {
                QString temp_dir = QCoreApplication::applicationDirPath() + "/cache/temp";
                QDir().mkpath(temp_dir);
                QString temp_file = temp_dir + "/clipboard_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
                if (img.save(temp_file, "PNG")) {
                    chat_area_set_pending_file(*c, temp_file);
                    return true;
                }
            }
        }
        return false;
    };

    key_filter->on_submit = [chat_widget]() {
        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (!c || !c->input) return;
        uint64_t r_id = c->current_room_id();
        if (r_id == 0) return;

        QString text = c->input->toPlainText().trimmed();
        bool has_text = !text.isEmpty();
        bool has_file = !c->pending_file.local_path.isEmpty() && QFile::exists(c->pending_file.local_path);

        if (!has_text && !has_file) return;

        if (has_text) {
            chat_rate_limiter_t &limiter = get_chat_rate_limiter();
            qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
            if (now_ms < limiter.timeout_until) {
                int sec = (int)((limiter.timeout_until - now_ms + 999) / 1000);
                show_toast(chat_widget, QString("Rate limit exceeded. Please wait %1s before sending again.").arg(sec), "error");
                return;
            }
            while (!limiter.timestamps.isEmpty() && limiter.timestamps.first() < now_ms - 2000) {
                limiter.timestamps.removeFirst();
            }
            if (limiter.timestamps.size() >= 5) {
                limiter.timeout_until = now_ms + 5000;
                limiter.timestamps.clear();
                show_toast(chat_widget, "You are sending messages too fast! Timed out for 5 seconds.", "error");
                return;
            }
            limiter.timestamps.append(now_ms);

            user_send_chat_message(r_id, text);
            c->input->clear();
            adjust_input_height(c->input, 36, 130);
        }

        if (has_file) {
            QString path_to_upload = c->pending_file.local_path;
            chat_area_clear_pending_file(*c);
            user_upload_file(r_id, path_to_upload);
        }
    };
    chat.input->installEventFilter(key_filter);

    ChatDropFilter *drop_filter = new ChatDropFilter(chat.widget);
    drop_filter->on_file_dropped = [chat_widget](const QString &file_path) {
        chat_area_t *c = (chat_area_t*)chat_widget->property("chat_area_ptr").value<void*>();
        if (c) chat_area_set_pending_file(*c, file_path);
    };
    chat.widget->installEventFilter(drop_filter);

    return chat;
}

#endif // CHAT_AREA_H
