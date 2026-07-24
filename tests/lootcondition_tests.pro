QT += core
QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = lootcondition_tests
TEMPLATE = app

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/lootcondition_tests.cpp \
    $$PWD/../src/lootcondition.cpp

HEADERS += \
    $$PWD/../src/lootcondition.h \
    $$PWD/../cubiomes/finders.h \
    $$PWD/../cubiomes/loot.h

LIBS += $$PWD/../cubiomes/libcubiomes.a -lm
