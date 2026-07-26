QT += core
QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = village_loot_seed_probe
TEMPLATE = app

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/village_loot_seed_probe.cpp \
    $$PWD/../src/villagelootseed.cpp \
    $$PWD/../src/villagestructure.cpp

HEADERS += \
    $$PWD/../src/villagelootseed.h \
    $$PWD/../src/villagestructure.h \
    $$PWD/../cubiomes/finders.h \
    $$PWD/../cubiomes/generator.h \
    $$PWD/../cubiomes/noise.h

LIBS += $$PWD/../cubiomes/libcubiomes.a -lm
