#ifndef ROOM_MODEL_H
#define ROOM_MODEL_H

#include <QAbstractItemModel>
#include <QSet>
#include <memory>
#include <vector>
#include "common/types.h"

enum room_model_role_t {
    ROOM_NODE_TYPE_ROLE = Qt::UserRole + 1,
    ROOM_ID_ROLE,
    ROOM_NAME_ROLE,
    ROOM_PASSWORD_ROLE,
    ROOM_USER_ID_ROLE,
    ROOM_AVATAR_ROLE,
    ROOM_SPEAKING_ROLE,
    ROOM_MUTED_ROLE,
    ROOM_DEAFENED_ROLE,
    ROOM_USER_COUNT_ROLE
};

class RoomTreeModel : public QAbstractItemModel {
    struct node_t {
        room_t room;
        voice_user_t user;
        node_t *parent = nullptr;
        std::vector<std::unique_ptr<node_t>> children;
        bool is_room = false;
        bool is_speaking = false;
    };

public:
    explicit RoomTreeModel(QObject *parent = nullptr)
        : QAbstractItemModel(parent) {}

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (column != 0 || row < 0) return {};
        const auto &siblings = parent.isValid()
                                  ? static_cast<node_t *>(parent.internalPointer())->children
                                  : rooms_;
        if (static_cast<size_t>(row) >= siblings.size()) return {};
        return createIndex(row, column, siblings[row].get());
    }

    QModelIndex parent(const QModelIndex &child) const override {
        if (!child.isValid()) return {};
        node_t *node = static_cast<node_t *>(child.internalPointer());
        if (!node->parent || !node->parent->parent) return {};
        node_t *parent_node = node->parent;
        for (size_t row = 0; row < rooms_.size(); ++row) {
            if (rooms_[row].get() == parent_node) {
                return createIndex(static_cast<int>(row), 0, parent_node);
            }
        }
        return {};
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.column() > 0) return 0;
        if (!parent.isValid()) return static_cast<int>(rooms_.size());
        return static_cast<int>(
            static_cast<node_t *>(parent.internalPointer())->children.size());
    }

    int columnCount(const QModelIndex & = QModelIndex()) const override {
        return 1;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid()) return {};
        const node_t *node = static_cast<const node_t *>(index.internalPointer());
        if (role == ROOM_NODE_TYPE_ROLE) return node->is_room;
        if (node->is_room) {
            if (role == Qt::DisplayRole || role == ROOM_NAME_ROLE) return node->room.name;
            if (role == ROOM_ID_ROLE) return QVariant::fromValue<qulonglong>(node->room.id);
            if (role == ROOM_PASSWORD_ROLE) return node->room.has_password;
            if (role == ROOM_USER_COUNT_ROLE) {
                return static_cast<int>(node->room.users.size());
            }
            return {};
        }

        if (role == Qt::DisplayRole) return node->user.name;
        if (role == ROOM_ID_ROLE) return QVariant::fromValue<qulonglong>(node->parent->room.id);
        if (role == ROOM_USER_ID_ROLE) return QVariant::fromValue<qulonglong>(node->user.id);
        if (role == ROOM_NAME_ROLE) return node->user.name;
        if (role == ROOM_AVATAR_ROLE) return node->user.avatar;
        if (role == ROOM_SPEAKING_ROLE) return node->is_speaking;
        if (role == ROOM_MUTED_ROLE) return node->user.is_mute;
        if (role == ROOM_DEAFENED_ROLE) return node->user.is_deaf;
        return {};
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override {
        if (!index.isValid() || role != ROOM_SPEAKING_ROLE) return false;
        node_t *node = static_cast<node_t *>(index.internalPointer());
        if (node->is_room || node->is_speaking == value.toBool()) return false;
        node->is_speaking = value.toBool();
        emit dataChanged(index, index, {ROOM_SPEAKING_ROLE});
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

    void set_rooms(const QList<room_t> &rooms, const QSet<uint64_t> &active_speakers) {
        bool same_structure = rooms.size() == static_cast<int>(rooms_.size());
        for (int room_row = 0; same_structure && room_row < rooms.size(); ++room_row) {
            const node_t *old_room = rooms_[room_row].get();
            const room_t &new_room = rooms[room_row];
            same_structure = old_room->room.id == new_room.id &&
                             old_room->children.size() ==
                                 static_cast<size_t>(new_room.users.size());
            for (int user_row = 0; same_structure && user_row < new_room.users.size(); ++user_row) {
                same_structure = old_room->children[user_row]->user.id ==
                                 new_room.users[user_row].id;
            }
        }

        if (same_structure) {
            for (int room_row = 0; room_row < rooms.size(); ++room_row) {
                node_t *room_node = rooms_[room_row].get();
                room_node->room = rooms[room_row];
                QModelIndex room_index = index(room_row, 0);
                emit dataChanged(room_index, room_index);
                for (int user_row = 0; user_row < rooms[room_row].users.size(); ++user_row) {
                    node_t *user_node = room_node->children[user_row].get();
                    user_node->user = rooms[room_row].users[user_row];
                    user_node->is_speaking = active_speakers.contains(user_node->user.id);
                    QModelIndex user_index = index(user_row, 0, room_index);
                    emit dataChanged(user_index, user_index);
                }
            }
            return;
        }

        beginResetModel();
        rooms_.clear();
        for (const room_t &room : rooms) {
            auto room_node = std::make_unique<node_t>();
            room_node->room = room;
            room_node->is_room = true;
            room_node->parent = &root_;
            for (const voice_user_t &user : room.users) {
                auto user_node = std::make_unique<node_t>();
                user_node->user = user;
                user_node->is_speaking = active_speakers.contains(user.id);
                user_node->parent = room_node.get();
                room_node->children.push_back(std::move(user_node));
            }
            rooms_.push_back(std::move(room_node));
        }
        endResetModel();
    }

    void set_active_speakers(const QSet<uint64_t> &active_speakers) {
        for (size_t room_row = 0; room_row < rooms_.size(); ++room_row) {
            QModelIndex room_index = index(static_cast<int>(room_row), 0);
            node_t *room_node = rooms_[room_row].get();
            for (size_t user_row = 0; user_row < room_node->children.size(); ++user_row) {
                node_t *user_node = room_node->children[user_row].get();
                bool is_speaking = active_speakers.contains(user_node->user.id);
                if (user_node->is_speaking == is_speaking) continue;
                user_node->is_speaking = is_speaking;
                QModelIndex user_index = index(static_cast<int>(user_row), 0, room_index);
                emit dataChanged(user_index, user_index, {ROOM_SPEAKING_ROLE});
            }
        }
    }

private:
    node_t root_;
    std::vector<std::unique_ptr<node_t>> rooms_;
};

#endif // ROOM_MODEL_H
