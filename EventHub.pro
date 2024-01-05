TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt
LIBS +=  -lpthread
SOURCES += \
        eventhub.cpp \
        main.cpp

HEADERS += \
    eventhub.h

    win32 {
        DEFINES  += OS_WIN
        LIBS += -lodbc32
    }
    unix {
        DEFINES += OS_UNIX
        LIBS += -lodbc
    }
