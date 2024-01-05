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
}
unix {
        DEFINES += OS_UNIX
}

