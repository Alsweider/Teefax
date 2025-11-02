TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lwinmm

VERSION = 0.6.2
DEFINES += PRG_VERSION=\\\"$$VERSION\\\"


SOURCES += \
        main.cpp

HEADERS += \
    sound_array.h
