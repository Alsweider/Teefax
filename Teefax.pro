TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lwinmm

VERSION = 0.23.3
DEFINES += PRG_VERSION=\\\"$$VERSION\\\" \

QMAKE_TARGET_COMPANY     = "Alsweider"
QMAKE_TARGET_PRODUCT     = "Teefax"
QMAKE_TARGET_DESCRIPTION = "Teefax CLI Timer & Utilities"
QMAKE_TARGET_COPYRIGHT   = "Copyright 2025 - 2026 Alsweider | github.com/Alsweider/Teefax"

RC_ICONS = icon.ico

SOURCES += \
        main.cpp

HEADERS += \
    i18n.h \
    sound_array.h
