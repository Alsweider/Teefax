TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt
# QT += core

LIBS += -lwinmm

VERSION = 0.27.11
DEFINES += PRG_VERSION=\\\"$$VERSION\\\" \

QMAKE_TARGET_COMPANY     = "Alsweider"
QMAKE_TARGET_PRODUCT     = "Teefax"
QMAKE_TARGET_DESCRIPTION = "Teefax CLI Timer & Utilities"
QMAKE_TARGET_COPYRIGHT   = "Copyright 2025 - 2026 Alsweider | github.com/Alsweider/Teefax"
QMAKE_LFLAGS += -static-libgcc -static-libstdc++ -static

RC_ICONS = icon_256.ico

SOURCES += \
        main.cpp

HEADERS += \
    i18n.h \
    sound_array.h
