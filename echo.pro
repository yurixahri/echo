QT += core gui widgets svg network

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
QMAKE_LIBS += -ltasn1 -lz -lharfbuzz -lgraphite2 -lbz2 -lusp10 -lrpcrt4

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
    ui/qcollapsewidget.cpp \
    utils/config.cpp

HEADERS += \
    common/types.h \
    functions/user.h \
    mainwindow.h \
    ui/qcollapsewidget.h \
    utils/asn1_parser.h \
    utils/config.h \
    utils/theme.h \
    utils/zlib_helper.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
