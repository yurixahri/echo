QT += core gui widgets svg network multimedia

CONFIG += c++20
TARGET = Echo
win32: RC_ICONS = assets/icons/logo_small.ico

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
QMAKE_LIBS += -ltasn1 -lopus -lz -lharfbuzz -lgraphite2 -lbz2 -lusp10 -lrpcrt4 -lavrt -lksuser -lpropsys

SOURCE_ASSETS_DIR = $$PWD/assets
SOURCE_STYLES_DIR = $$PWD/styles
SOURCE_ASN1_DEF = $$PWD/ChatProtocol.asn
CONFIG(debug, debug|release) {
    DEST_ASSETS_DIR = $$OUT_PWD/debug/assets
    DEST_STYLES_DIR = $$OUT_PWD/debug/styles
    DEST_DIR = $$OUT_PWD/debug
} else {
    DEST_ASSETS_DIR = $$OUT_PWD/release/assets
    DEST_STYLES_DIR = $$OUT_PWD/release/styles
    DEST_DIR = $$OUT_PWD/release
}

win32 {
    SOURCE_ASSETS_DIR ~= s,/,\\,g
    SOURCE_STYLES_DIR ~= s,/,\\,g
    DEST_ASSETS_DIR   ~= s,/,\\,g
    DEST_STYLES_DIR   ~= s,/,\\,g
    DEST_DIR ~= s,/,\\,g
    SOURCE_ASN1_DEF ~= s,/,\\,g
}

copy_styles.commands = $(COPY_DIR) $$SOURCE_STYLES_DIR $$DEST_STYLES_DIR
copy_assets.commands = $(COPY_DIR) $$SOURCE_ASSETS_DIR $$DEST_ASSETS_DIR
copy_asn1.commands = $(COPY_FILE) $$SOURCE_ASN1_DEF $$DEST_DIR

QMAKE_EXTRA_TARGETS += copy_asn1 copy_styles copy_assets
POST_TARGETDEPS += copy_asn1 copy_styles copy_assets


SOURCES += \
    main.cpp \
    mainwindow.cpp \
    ui/popup.cpp \
    ui/qcollapsewidget.cpp \
    utils/config.cpp

HEADERS += \
    common/types.h \
    utils/asn1_parser.h \
    utils/config.h \
    utils/markdown_helper.h \
    utils/theme.h \
    utils/zlib_helper.h \
    functions/file.h \
    functions/role.h \
    functions/room.h \
    functions/user.h \
    functions/voice.h \
    functions/sound.h \
    ui/toast.h \
    mainwindow.h \
    ui/chat_area.h \
    ui/claim_admin.h \
    ui/connection_popup.h \
    ui/create_room.h \
    ui/my_info.h \
    ui/password_popup.h \
    ui/popup.h \
    ui/qcollapsewidget.h \
    ui/roles_popup.h \
    ui/room_settings_popup.h \
    ui/room_view.h \
    ui/server_header.h \
    ui/server_list.h \
    ui/server_settings.h \
    ui/settings_popup.h \
    ui/user_roles_popup.h \
    ui/user_info_popup.h \
    ui/user_volume_popup.h \
    ui/server_users_popup.h \
    ui/voice_panel.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
