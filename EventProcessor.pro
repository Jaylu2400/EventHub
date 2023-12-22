TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt
LIBS += -ldbus-1\
            -lpthread
SOURCES += \
        eventhub.cpp \
        main.cpp

HEADERS += \
    eventhub.h
