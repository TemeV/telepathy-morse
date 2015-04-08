TEMPLATE = app
TARGET   = telepathy-morse

QT = core dbus xml network

INSTALLS -= target
target.path = /usr/libexec
INSTALLS += target

manager.files = morse.manager
manager.path = /usr/share/telepathy/managers
INSTALLS += manager

service.files = org.freedesktop.Telepathy.ConnectionManager.morse.service
service.path = /usr/share/dbus-1/services
INSTALLS += service

INCLUDEPATH += /usr/local/include/telepathy-qt5
INCLUDEPATH += /usr/local/include/telegram-qt5

LIBS += -lcrypto
LIBS += -lz
LIBS += -L/usr/local/lib
LIBS += -ltelegram-qt5
LIBS += -ltelepathy-qt5-service
LIBS += -ltelepathy-qt5

SOURCES = \
    main.cpp \
    connection.cpp \
    protocol.cpp \
    textchannel.cpp

HEADERS = \
    connection.hpp \
    protocol.hpp \
    textchannel.hpp

OTHER_FILES += \
    rpm/telepathy-morse.yaml \
    rpm/telepathy-morse.spec

