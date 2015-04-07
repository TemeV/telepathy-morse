
QT = core dbus xml network

TEMPLATE = app
TARGET   = telepathy-morse

#CONFIG  += dll
#DEFINES += TELEGRAMQT_LIBRARY

SOURCES = \
    main.cpp \
    connection.cpp \
    protocol.cpp \
    textchannel.cpp

HEADERS = \
    connection.hpp \
    protocol.hpp \
    textchannel.hpp


INSTALLS -= target
#target.files = $$TARGET
target.path = /usr/local/libexec
INSTALLS += target


INCLUDEPATH += /usr/local/include/telepathy-qt5
INCLUDEPATH += /usr/local/include/telegram-qt5

#DEPENDPATH += /usr/local/lib

LIBS += -lcrypto
LIBS += -lz
LIBS += -L/usr/local/lib
LIBS += -ltelegram-qt5
LIBS += -ltelepathy-qt5-service
LIBS += -ltelepathy-qt5

#unix: CONFIG += link_pkgconfig
#unix: PKGCONFIG += telepathy-qt5

OTHER_FILES += \
    rpm/telepathy-morse.yaml

