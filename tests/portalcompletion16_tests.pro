QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = portalcompletion16_tests
TEMPLATE = app

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/portalcompletion16_tests.cpp \
    $$PWD/../src/portalcompletion16.cpp \
    $$PWD/../src/terrainoracle26.cpp

HEADERS += \
    $$PWD/../src/portalcompletion16.h \
    $$PWD/../src/terrainoracle26.h \
    $$PWD/../cubiomes/finders.h \
    $$PWD/../cubiomes/generator.h \
    $$PWD/../cubiomes/loot.h

LIBS += $$PWD/../cubiomes/libcubiomes.a -lm
