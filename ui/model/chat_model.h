#ifndef CHAT_MODEL_H
#define CHAT_MODEL_H

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCache>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHash>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include "common/types.h"
#include "functions/file.h"
#include "utils/markdown_helper.h"
#include "utils/theme.h"

enum chat_model_role_t {
    CHAT_MESSAGE_ID_ROLE = Qt::UserRole + 1,
    CHAT_SENDER_ID_ROLE,
    CHAT_SENDER_ROLE,
    CHAT_AVATAR_ROLE,
    CHAT_CONTENT_ROLE,
    CHAT_TYPE_ROLE,
    CHAT_TIMESTAMP_ROLE,
    CHAT_ROOM_ID_ROLE
};

class ChatMessageModel : public QAbstractListModel {
public:
    explicit ChatMessageModel(QObject *parent = nullptr)
        : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : messages_.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= messages_.size()) return {};
        const chat_message_t &message = messages_[index.row()];
        switch (role) {
            case Qt::DisplayRole:
            case CHAT_CONTENT_ROLE: return message.msg;
            case Qt::ToolTipRole:
                if (message.msg_type == MESSAGE_TYPE::VIDEO) {
                    return QString("Right-click for play, save, and folder actions");
                }
                if (message.msg_type == MESSAGE_TYPE::OTHER) {
                    return QString("Right-click for open, save, and folder actions");
                }
                return {};
            case CHAT_MESSAGE_ID_ROLE: return QVariant::fromValue<qulonglong>(message.id);
            case CHAT_SENDER_ID_ROLE: return QVariant::fromValue<qulonglong>(message.sender_id);
            case CHAT_SENDER_ROLE: return message.sender;
            case CHAT_AVATAR_ROLE: return message.sender_avatar;
            case CHAT_TYPE_ROLE: return message.msg_type;
            case CHAT_TIMESTAMP_ROLE: return message.timestamp;
            case CHAT_ROOM_ID_ROLE: return QVariant::fromValue<qulonglong>(message.room_id);
            default: return {};
        }
    }

    void clear() {
        if (messages_.isEmpty()) return;
        beginResetModel();
        messages_.clear();
        endResetModel();
    }

    void append(const chat_message_t &message) {
        request_assets(message);
        int row = messages_.size();
        beginInsertRows(QModelIndex(), row, row);
        messages_.append(message);
        endInsertRows();
    }

    void append_batch(const QList<chat_message_t> &messages) {
        if (messages.isEmpty()) return;
        for (const chat_message_t &message : messages) request_assets(message);
        int first_row = messages_.size();
        int last_row = first_row + messages.size() - 1;
        beginInsertRows(QModelIndex(), first_row, last_row);
        for (const chat_message_t &message : messages) messages_.append(message);
        endInsertRows();
    }

    void prepend(const QList<chat_message_t> &messages) {
        if (messages.isEmpty()) return;
        for (const chat_message_t &message : messages) request_assets(message);
        int count = messages.size();
        beginInsertRows(QModelIndex(), 0, count - 1);
        for (int i = count - 1; i >= 0; --i) messages_.prepend(messages[i]);
        endInsertRows();
    }

    bool remove_message(uint64_t message_id) {
        if (message_id == 0) return false;
        for (int row = 0; row < messages_.size(); ++row) {
            if (messages_[row].id != message_id) continue;
            beginRemoveRows(QModelIndex(), row, row);
            messages_.removeAt(row);
            endRemoveRows();
            return true;
        }
        return false;
    }

    QString message_content(uint64_t message_id) const {
        for (const chat_message_t &message : messages_) {
            if (message.id == message_id) return message.msg;
        }
        return {};
    }

    uint16_t message_type(uint64_t message_id) const {
        for (const chat_message_t &message : messages_) {
            if (message.id == message_id) return message.msg_type;
        }
        return MESSAGE_TYPE::TEXT;
    }

    void avatar_ready(const QString &avatar_path) {
        requested_avatars_.remove(avatar_path);
    }

    void refresh_file(const QString &file_path) {
        for (int row = 0; row < messages_.size(); ++row) {
            QString content = messages_[row].msg;
            content.replace("\\", "/");
            if (content == file_path) emit dataChanged(index(row), index(row), {});
        }
    }

private:
    void request_assets(const chat_message_t &message) {
        QString avatar_path = message.sender_avatar;
        if (avatar_path.isEmpty() &&
            message.sender_id == static_cast<uint64_t>(client.user_id)) {
            avatar_path = client.user_avatar;
        }
        if (!avatar_path.isEmpty() && !runtime_avatar_pixmaps.contains(avatar_path) &&
            !requested_avatars_.contains(avatar_path)) {
            requested_avatars_.insert(avatar_path);
            user_request_file_download(0, avatar_path, 0);
        }

        if (message.msg_type != MESSAGE_TYPE::IMAGE) return;
        QString image_path = message.msg;
        image_path.replace("\\", "/");
        if (!image_path.isEmpty() && get_cached_file_path(image_path).isEmpty() &&
            !requested_files_.contains(image_path)) {
            requested_files_.insert(image_path);
            user_request_file_download(message.room_id, image_path, 0);
        }
    }

    QList<chat_message_t> messages_;
    QSet<QString> requested_avatars_;
    QSet<QString> requested_files_;
};

class ChatMessageDelegate : public QStyledItemDelegate {
    struct layout_t {
        bool mine = false;
        bool group_start = true;
        bool date_start = false;
        int row_height = 0;
        QString date_text;
        QRect date_rect;
        QRect avatar_rect;
        QRect sender_rect;
        QRect bubble_rect;
        QRect time_rect;
        QRect image_rect;
        QRect file_name_rect;
    };

    struct movie_entry_t {
        QMovie *movie = nullptr;
        QPersistentModelIndex index;
        QString path;
        QSize scaled_size;
    };

public:
    explicit ChatMessageDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {
        auto *view = qobject_cast<QAbstractItemView *>(parent);
        if (view) {
            time_style_ = create_style_label(view, "chat_msg_time");
            sender_style_ = create_style_label(view, "chat_msg_sender");
            date_style_ = create_style_label(view, "chat_date_label");
            content_style_ = create_style_label(view, "chat_msg_content");
            file_name_style_ = create_style_label(view, "chat_file_name");
            bubble_me_style_ = create_style_widget(view, "chat_msg_bubble_me");
            bubble_other_style_ = create_style_widget(view, "chat_msg_bubble_other");
            date_line_style_ = new QFrame(view);
            date_line_style_->setObjectName("chat_date_line");
            date_line_style_->setAttribute(Qt::WA_StyledBackground, true);
            date_line_style_->hide();
        }
        if (view && view->verticalScrollBar()) {
            QObject::connect(view->verticalScrollBar(), &QScrollBar::valueChanged,
                             this, [this]() { update_movies(); });
            QObject::connect(view->verticalScrollBar(), &QScrollBar::rangeChanged,
                             this, [this]() { update_movies(); });
            if (view->model()) {
                QAbstractItemModel *model = view->model();
                QObject::connect(model, &QAbstractItemModel::modelReset,
                                 this, [this]() { update_movies(); });
                QObject::connect(model, &QAbstractItemModel::modelReset,
                                 this, [this]() { layout_cache_.clear(); });
                QObject::connect(model, &QAbstractItemModel::rowsInserted,
                    this, [this](const QModelIndex &, int first, int) {
                        if (first == 0) layout_cache_.clear();
                    });
                QObject::connect(model, &QAbstractItemModel::rowsRemoved,
                    this, [this](const QModelIndex &, int, int) {
                        layout_cache_.clear();
                    });
                QObject::connect(model, &QAbstractItemModel::dataChanged,
                    this, [this]() { layout_cache_.clear(); });
            }
        }
    }

    void update_visible_movies() const {
        update_movies();
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        chat_message_t message = message_at(index);
        layout_t layout = calculate_layout(option, index, message);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (layout.group_start) {
            draw_avatar(painter, layout.avatar_rect, message.sender_avatar, layout.mine);
            if (!layout.mine) {
                painter->setFont(style_font(sender_style_, option.font));
                painter->setPen(style_color(sender_style_, global_theme.primary));
                painter->drawText(layout.sender_rect, Qt::AlignLeft | Qt::AlignVCenter,
                                  message.sender);
            }
        }

        if (layout.date_start) {
            painter->setFont(style_font(date_style_, option.font));
            painter->setPen(style_color(date_style_, global_theme.muted_foreground));
            int text_width = painter->fontMetrics().horizontalAdvance(layout.date_text);
            int text_left = layout.date_rect.center().x() - text_width / 2;
            draw_style_widget(painter, date_line_style_,
                QRect(layout.date_rect.left(), layout.date_rect.center().y(),
                      qMax(0, text_left - 8 - layout.date_rect.left()), 1));
            painter->drawText(layout.date_rect, Qt::AlignCenter, layout.date_text);
            draw_style_widget(painter, date_line_style_,
                QRect(text_left + text_width + 8, layout.date_rect.center().y(),
                      qMax(0, layout.date_rect.right() -
                          (text_left + text_width + 8) + 1), 1));
        }

        draw_style_widget(painter,
                          layout.mine ? bubble_me_style_ : bubble_other_style_,
                          layout.bubble_rect);

        if (message.msg_type == MESSAGE_TYPE::TEXT) {
            QRect inner_rect = layout.bubble_rect.adjusted(6, 6, -6, -6);
            QTextDocument *document = document_for(message, inner_rect.width());
            painter->save();
            painter->setClipRect(inner_rect, Qt::IntersectClip);
            painter->translate(inner_rect.topLeft());
            document->drawContents(painter,
                                   QRectF(0, 0, inner_rect.width(), inner_rect.height()));
            painter->restore();
        } else if (message.msg_type == MESSAGE_TYPE::IMAGE) {
            QRect inner_rect = layout.bubble_rect.adjusted(6, 6, -6, -6);
            QString path = normalized_path(message.msg);
            QString cached = get_cached_file_path(path);
            QPixmap image = path.toLower().endsWith(".gif")
                ? movie_pixmap(message, cached, layout.image_rect.size(), index)
                : image_pixmap(cached, layout.image_rect.size());
            if (!image.isNull()) {
                painter->drawPixmap(layout.image_rect.topLeft(), image);
            } else {
                painter->setFont(option.font);
                painter->setPen(QColor(global_theme.muted_foreground));
                painter->drawText(inner_rect, Qt::AlignLeft | Qt::AlignVCenter,
                                  QString("Image: %1").arg(QFileInfo(path).fileName()));
            }
        } else {
            QString path = normalized_path(message.msg);
            painter->setFont(style_font(file_name_style_, option.font));
            painter->setPen(style_color(file_name_style_, global_theme.foreground));
            QString filename = painter->fontMetrics().elidedText(
                QFileInfo(path).fileName(), Qt::ElideMiddle,
                layout.file_name_rect.width());
            painter->drawText(layout.file_name_rect, Qt::AlignLeft | Qt::AlignVCenter,
                              filename);
        }

        painter->setFont(style_font(time_style_, option.font));
        painter->setPen(style_color(time_style_, global_theme.muted_foreground));
        painter->drawText(layout.time_rect, Qt::AlignVCenter |
                          (layout.mine ? Qt::AlignRight : Qt::AlignLeft),
                          message.timestamp.toString("HH:mm:ss"));
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        auto *mouse_event = static_cast<QMouseEvent *>(event);
        if (mouse_event->button() != Qt::LeftButton) return false;

        chat_message_t message = message_at(index);
        layout_t layout = calculate_layout(option, index, message);
        QPoint position = mouse_event->pos();
        if (message.msg_type == MESSAGE_TYPE::TEXT) {
            QRect inner_rect = layout.bubble_rect.adjusted(6, 6, -6, -6);
            if (!inner_rect.contains(position)) return false;
            QTextDocument *document = document_for(message, inner_rect.width());
            QString link = document->documentLayout()->anchorAt(
                position - inner_rect.topLeft());
            if (link.isEmpty()) return false;
            QDesktopServices::openUrl(QUrl(link));
            return true;
        }

        QString path = normalized_path(message.msg);
        if (message.msg_type == MESSAGE_TYPE::IMAGE) {
            QString cached = get_cached_file_path(path);
            if (!cached.isEmpty() && layout.image_rect.contains(position)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(cached));
                return true;
            }
            return false;
        }

        return false;
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
        layout_t layout = calculate_layout(option, index, message_at(index));
        auto *view = qobject_cast<QAbstractItemView *>(parent());
        int width = view ? view->viewport()->width() : option.rect.width();
        return QSize(qMax(width, option.rect.width()), layout.row_height);
    }

private:
    static QString normalized_path(QString path) {
        path.replace("\\", "/");
        return path;
    }

    static chat_message_t message_at(const QModelIndex &index) {
        chat_message_t message;
        message.id = index.data(CHAT_MESSAGE_ID_ROLE).toULongLong();
        message.room_id = index.data(CHAT_ROOM_ID_ROLE).toULongLong();
        message.sender_id = index.data(CHAT_SENDER_ID_ROLE).toULongLong();
        message.sender = index.data(CHAT_SENDER_ROLE).toString();
        message.sender_avatar = index.data(CHAT_AVATAR_ROLE).toString();
        message.msg = index.data(CHAT_CONTENT_ROLE).toString();
        message.msg_type = index.data(CHAT_TYPE_ROLE).toUInt();
        message.timestamp = index.data(CHAT_TIMESTAMP_ROLE).toDateTime();
        return message;
    }

    bool is_group_start(const QModelIndex &index,
                        const chat_message_t &message) const {
        if (index.row() == 0) return true;
        QModelIndex previous = index.sibling(index.row() - 1, index.column());
        return previous.data(CHAT_SENDER_ID_ROLE).toULongLong() != message.sender_id ||
               previous.data(CHAT_TIMESTAMP_ROLE).toDateTime().date() !=
                   message.timestamp.date();
    }

    layout_t calculate_layout(const QStyleOptionViewItem &option,
                              const QModelIndex &index,
                              const chat_message_t &message) const {
        int item_width = option.rect.width();
        if (item_width <= 0) {
            auto *view = qobject_cast<QAbstractItemView *>(parent());
            item_width = view ? view->viewport()->width() : 0;
        }

        quint64 cache_key =
            (static_cast<quint64>(static_cast<quint32>(item_width)) << 32) |
            static_cast<quint32>(index.row());
        cache_key ^= qHash(option.font.toString());
        cache_key ^= static_cast<quint64>(
            qHash(style_font(time_style_, option.font).toString())) << 1;
        cache_key ^= static_cast<quint64>(
            qHash(style_font(sender_style_, option.font).toString())) << 17;
        cache_key ^= static_cast<quint64>(
            qHash(style_font(date_style_, option.font).toString())) << 33;
        cache_key ^= static_cast<quint64>(
            qHash(style_font(content_style_, option.font).toString())) << 9;
        cache_key ^= static_cast<quint64>(
            qHash(style_font(file_name_style_, option.font).toString())) << 25;
        auto cached = layout_cache_.constFind(cache_key);
        QPoint origin = option.rect.topLeft();
        if (cached != layout_cache_.cend()) {
            layout_t layout = cached.value();
            translate_layout(layout, origin);
            return layout;
        }

        QStyleOptionViewItem normalized_option(option);
        normalized_option.rect.setRect(0, 0, item_width, option.rect.height());
        layout_t layout = calculate_layout_uncached(
            normalized_option, index, message);
        layout_cache_.insert(cache_key, layout);
        translate_layout(layout, origin);
        return layout;
    }

    layout_t calculate_layout_uncached(const QStyleOptionViewItem &option,
                                       const QModelIndex &index,
                                       const chat_message_t &message) const {
        layout_t layout;
        layout.mine = message.sender_id > 0 &&
                      message.sender_id == static_cast<uint64_t>(client.user_id);
        layout.group_start = is_group_start(index, message);
        int item_width = option.rect.width();
        if (item_width <= 0) {
            auto *view = qobject_cast<QAbstractItemView *>(parent());
            item_width = view ? view->viewport()->width() : 0;
        }
        QRect item_rect = option.rect;
        if (item_rect.width() <= 0) item_rect.setWidth(item_width);
        QRect row = item_rect.adjusted(6, 3, -6, -3);
        QFontMetrics time_metrics(style_font(time_style_, option.font));
        constexpr int avatar_size = 34;
        int time_width = time_metrics.horizontalAdvance("00:00:00") + 8;
        int content_left = row.left() + avatar_size + 8;
        int content_right = row.right() - avatar_size - 8;
        int available_bubble_width = row.width() - avatar_size - 8 - time_width - 8;
        int max_bubble_width = qMax(
            70, qMin(int(item_width * 0.70), available_bubble_width));
        int date_height = QFontMetrics(style_font(date_style_, option.font)).height();
        int sender_height = layout.group_start && !layout.mine
            ? QFontMetrics(style_font(sender_style_, option.font)).height()
            : 0;
        int file_name_height = QFontMetrics(
            style_font(file_name_style_, option.font)).height();

        layout.date_start = index.row() == 0 ||
            index.sibling(index.row() - 1, index.column())
                .data(CHAT_TIMESTAMP_ROLE).toDateTime().date() !=
                    message.timestamp.date();
        int y = row.top();
        if (layout.date_start) {
            layout.date_text = message.timestamp.date().toString("MMMM d, yyyy");
            layout.date_rect = QRect(row.left(), y, row.width(), date_height);
            y += date_height + 4;
        }
        layout.sender_rect = QRect(content_left, y, max_bubble_width,
                                   qMax(sender_height, 1));
        y += sender_height;

        int bubble_width = 0;
        int content_height = 0;
        bool text_message = message.msg_type == MESSAGE_TYPE::TEXT;
        QString path = normalized_path(message.msg);
        if (text_message) {
            int max_text_width = qMax(1, max_bubble_width - 12);
            QTextDocument *measure_doc = document_for(message, max_text_width);
            int text_width = qBound(1, qCeil(measure_doc->idealWidth()), max_text_width);
            QTextDocument *document = document_for(message, text_width);
            bubble_width = qMin(text_width + 12, max_bubble_width);
            content_height = qCeil(document->size().height());
        } else if (message.msg_type == MESSAGE_TYPE::IMAGE) {
            QSize bounds(qMax(50, max_bubble_width - 20), 220);
            QImageReader reader(get_cached_file_path(path));
            QSize source_size = reader.size();
            QSize image_size = source_size.isValid()
                ? source_size.scaled(bounds, Qt::KeepAspectRatio)
                : QSize(qMin(180, bounds.width()), 48);
            bubble_width = qMin(image_size.width() + 12, max_bubble_width);
            content_height = image_size.height();
            layout.image_rect = QRect(0, 0, image_size.width(), image_size.height());
        } else {
            QFontMetrics file_metrics(style_font(file_name_style_, option.font));
            int filename_width = file_metrics.horizontalAdvance(QFileInfo(path).fileName());
            bubble_width = qMin(filename_width + 12, max_bubble_width);
            content_height = file_name_height;
        }

        int bubble_x = layout.mine ? content_right - bubble_width : content_left;
        int bubble_padding = 12;
        layout.bubble_rect = QRect(bubble_x, y, bubble_width,
                                   content_height + bubble_padding);
        int row_content_height = qMax(layout.bubble_rect.height(),
                                      layout.group_start ? avatar_size : 0);
        layout.bubble_rect.moveTop(y + row_content_height -
                                   layout.bubble_rect.height());
        int time_x = layout.mine ? bubble_x - time_width - 8
                                 : bubble_x + bubble_width + 8;
        layout.time_rect = QRect(time_x,
                                 layout.bubble_rect.bottom() - time_metrics.height() + 1,
                                 time_width, time_metrics.height());
        layout.avatar_rect = layout.mine
            ? QRect(row.right() - avatar_size, y, avatar_size, avatar_size)
            : QRect(row.left(), y, avatar_size, avatar_size);

        QRect inner = layout.bubble_rect.adjusted(6, 6, -6, -6);
        if (message.msg_type == MESSAGE_TYPE::IMAGE) {
            layout.image_rect.moveTopLeft(inner.topLeft());
        } else if (message.msg_type != MESSAGE_TYPE::TEXT) {
            layout.file_name_rect = inner;
        }

        layout.row_height = (layout.date_start ? date_height + 4 : 0) + sender_height +
                            row_content_height + 6;
        return layout;
    }

    static void translate_layout(layout_t &layout, const QPoint &offset) {
        layout.date_rect.translate(offset);
        layout.avatar_rect.translate(offset);
        layout.sender_rect.translate(offset);
        layout.bubble_rect.translate(offset);
        layout.time_rect.translate(offset);
        layout.image_rect.translate(offset);
        layout.file_name_rect.translate(offset);
    }

    static QLabel *create_style_label(QWidget *parent, const char *object_name) {
        auto *label = new QLabel(parent);
        label->setObjectName(object_name);
        label->hide();
        return label;
    }

    static QWidget *create_style_widget(QWidget *parent, const char *object_name) {
        auto *widget = new QWidget(parent);
        widget->setObjectName(object_name);
        widget->setAttribute(Qt::WA_StyledBackground, true);
        widget->hide();
        return widget;
    }

    static QFont style_font(const QWidget *widget, const QFont &fallback) {
        return widget ? widget->font() : fallback;
    }

    static QColor style_color(const QWidget *widget, const QString &fallback) {
        return widget
            ? widget->palette().color(QPalette::WindowText)
            : QColor(fallback);
    }

    static void draw_style_widget(QPainter *painter, QWidget *widget,
                                  const QRect &rect) {
        if (!widget || rect.isEmpty()) return;
        QStyleOption style_option;
        style_option.initFrom(widget);
        style_option.rect = rect;
        widget->style()->drawPrimitive(QStyle::PE_Widget, &style_option,
                                       painter, widget);
    }

    void draw_avatar(QPainter *painter, const QRect &rect,
                     const QString &avatar_path, bool mine) const {
        QPixmap avatar = avatar_pixmap(avatar_path, mine);
        if (avatar.isNull()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(global_theme.border));
            painter->drawEllipse(rect);
            return;
        }

        QPixmap scaled = avatar.scaled(rect.size(), Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
        QRect source((scaled.width() - rect.width()) / 2,
                     (scaled.height() - rect.height()) / 2,
                     rect.width(), rect.height());
        QPainterPath avatar_clip;
        avatar_clip.addEllipse(rect);
        painter->save();
        painter->setClipPath(avatar_clip, Qt::IntersectClip);
        painter->drawPixmap(rect, scaled, source);
        painter->restore();
    }

    QTextDocument *document_for(const chat_message_t &message, int width) const {
        qulonglong key = message.id > 0 ? (message.id ^ (message.room_id * 0x9e3779b97f4a7c15ULL)) : qHash(message.msg);
        QTextDocument *document = documents_.object(key);
        if (!document) {
            document = new QTextDocument();
            QFont content_font = style_font(content_style_, QApplication::font());
            QColor content_color = style_color(content_style_, global_theme.foreground);
            document->setDefaultFont(content_font);
            document->setDocumentMargin(0);
            if (has_markdown_syntax(message.msg)) {
                document->setDefaultStyleSheet(
                    QString("body { color: %1; }")
                        .arg(content_color.name(QColor::HexArgb)));
                document->setHtml(render_markdown_to_html(message.msg, content_font));
            } else {
                QTextCharFormat format;
                format.setFont(content_font);
                format.setForeground(content_color);
                QTextCursor cursor(document);
                cursor.insertText(message.msg, format);
            }
            documents_.insert(key, document);
        }

        if (width > 0 && document->textWidth() != width) {
            document->setTextWidth(width);
        }
        return document;
    }

    static QPixmap avatar_pixmap(const QString &avatar_path, bool mine) {
        QString path = mine && avatar_path.isEmpty() ? client.user_avatar : avatar_path;
        if (runtime_avatar_pixmaps.contains(path)) return runtime_avatar_pixmaps.value(path);
        QPixmap pixmap(path);
        if (!pixmap.isNull()) return pixmap;
        return QPixmap(QCoreApplication::applicationDirPath() +
                       "/assets/defaults/user.png");
    }

    QPixmap image_pixmap(const QString &path, const QSize &bounds) const {
        if (path.isEmpty() || !QFile::exists(path)) return {};
        QString key = path + QStringLiteral("|%1x%2").arg(bounds.width()).arg(bounds.height());
        if (QPixmap *cached = images_.object(key)) return *cached;
        QImageReader reader(path);
        QSize source_size = reader.size();
        if (source_size.isValid()) {
            reader.setScaledSize(source_size.scaled(bounds, Qt::KeepAspectRatio));
        }
        QPixmap pixmap = QPixmap::fromImageReader(&reader);
        if (pixmap.width() > bounds.width() || pixmap.height() > bounds.height()) {
            pixmap = pixmap.scaled(bounds, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        if (!pixmap.isNull()) images_.insert(key, new QPixmap(pixmap));
        return pixmap;
    }

    QPixmap movie_pixmap(const chat_message_t &message, const QString &path,
                         const QSize &bounds, const QModelIndex &index) const {
        if (path.isEmpty() || !QFile::exists(path)) return {};
        qulonglong key = message.id ^ (message.room_id * 0x9e3779b97f4a7c15ULL);
        movie_entry_t &entry = movies_[key];
        QImageReader reader(path);
        QSize source_size = reader.size();
        QSize scaled_size = source_size.isValid()
            ? source_size.scaled(bounds, Qt::KeepAspectRatio) : bounds;
        if (!entry.movie || entry.path != path) {
            if (entry.movie) delete entry.movie;
            entry.movie = new QMovie(path, QByteArray(),
                                     const_cast<ChatMessageDelegate *>(this));
            entry.movie->setScaledSize(scaled_size);
            entry.index = QPersistentModelIndex(index);
            entry.path = path;
            entry.scaled_size = scaled_size;
            QObject::connect(entry.movie, &QMovie::frameChanged, this, [this, key]() {
                auto *view = qobject_cast<QAbstractItemView *>(parent());
                auto it = movies_.constFind(key);
                if (view && it != movies_.cend() && it->index.isValid()) {
                    view->viewport()->update(view->visualRect(it->index));
                }
            });
        } else if (entry.scaled_size != scaled_size) {
            entry.movie->setScaledSize(scaled_size);
            entry.scaled_size = scaled_size;
        }
        entry.index = QPersistentModelIndex(index);
        if (entry.movie->state() == QMovie::Paused) entry.movie->setPaused(false);
        else if (entry.movie->state() == QMovie::NotRunning) entry.movie->start();
        return entry.movie->currentPixmap();
    }

    void update_movies() const {
        auto *view = qobject_cast<QAbstractItemView *>(parent());
        if (!view) return;
        QRect viewport_rect = view->viewport()->rect();
        for (auto it = movies_.begin(); it != movies_.end(); ++it) {
            bool visible = view->isVisible() && it->index.isValid() &&
                           view->visualRect(it->index).intersects(viewport_rect);
            if (visible && it->movie->state() == QMovie::Paused) {
                it->movie->setPaused(false);
            } else if (!visible && it->movie->state() == QMovie::Running) {
                it->movie->setPaused(true);
            }
        }
    }

    mutable QCache<qulonglong, QTextDocument> documents_{256};
    mutable QCache<QString, QPixmap> images_{128};
    mutable QHash<qulonglong, movie_entry_t> movies_;
    mutable QHash<quint64, layout_t> layout_cache_;
    QLabel *time_style_ = nullptr;
    QLabel *sender_style_ = nullptr;
    QLabel *date_style_ = nullptr;
    QLabel *content_style_ = nullptr;
    QLabel *file_name_style_ = nullptr;
    QWidget *bubble_me_style_ = nullptr;
    QWidget *bubble_other_style_ = nullptr;
    QFrame *date_line_style_ = nullptr;
};

#endif
