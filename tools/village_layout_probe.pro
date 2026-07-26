QT += core
QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = village_layout_probe
TEMPLATE = app

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/village_layout_probe.cpp \
    $$PWD/../src/villagestructure.cpp

HEADERS += \
    $$PWD/../src/villagestructure.h \
    $$PWD/../cubiomes/finders.h \
    $$PWD/../cubiomes/generator.h \
    $$PWD/../cubiomes/loot.h

LIBS += $$PWD/../cubiomes/libcubiomes.a -lm
