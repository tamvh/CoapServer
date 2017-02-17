TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
DEFINES += WITH_POSIX
SOURCES += \
    main.cpp \
    zcoap.cpp \
    zmqtt.cpp

HEADERS += \
    zcoap.h \
    zmqtt.h

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libcoap/release/ -lcoap
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libcoap/debug/ -lcoap
else:unix: LIBS += -L$$OUT_PWD/../libcoap/ -lcoap

INCLUDEPATH += $$PWD/../libcoap
DEPENDPATH += $$PWD/../libcoap

INCLUDEPATH += /usr/local/include
LIBS += -L"/usr/local/lib" -lpaho-mqtt3a  -lpaho-mqtt3as -lpaho-mqtt3c -lpaho-mqtt3cs
