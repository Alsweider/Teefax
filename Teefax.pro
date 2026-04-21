TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lwinmm

VERSION = 0.20.1
DEFINES += PRG_VERSION=\\\"$$VERSION\\\" \

SOURCES += \
        main.cpp

HEADERS += \
    i18n.h \
    sound_array.h
