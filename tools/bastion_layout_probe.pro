QT += core
QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = bastion_layout_probe
TEMPLATE = app

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/bastion_layout_probe.cpp \
    $$PWD/../src/bastionstructure.cpp \
    $$PWD/../src/lootcondition.cpp \
    $$PWD/../src/villagelootseed.cpp \
    $$PWD/../src/villagestructure.cpp

HEADERS += \
    $$PWD/../src/bastionstructure.h \
    $$PWD/../src/lootcondition.h \
    $$PWD/../src/villagelootseed.h \
    $$PWD/../src/villagestructure.h \
    $$PWD/../cubiomes/finders.h \
    $$PWD/../cubiomes/loot.h

LIBS += $$PWD/../cubiomes/libcubiomes.a -lm
