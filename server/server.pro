include(../nymea.pri)

TARGET = guhd
TEMPLATE = app

INCLUDEPATH += ../libnymea ../libnymea-core

target.path = /usr/bin
INSTALLS += target

QT *= sql xml websockets bluetooth dbus network

LIBS += -L$$top_builddir/libnymea/ -lnymea \
        -L$$top_builddir/libnymea-core -lnymea-core \
        -lssl -lcrypto -laws-iot-sdk-cpp

# Server files
include(qtservice/qtservice.pri)

SOURCES += main.cpp \
    guhservice.cpp \
    guhapplication.cpp

HEADERS += \
    guhservice.h \
    guhapplication.h

