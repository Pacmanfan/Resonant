# Tests of the core library. They need no display and no OpenGL context.
include(../../warnings.pri)

TEMPLATE = app
TARGET = resonant_core_tests
CONFIG += console
CONFIG -= qt app_bundle

INCLUDEPATH += $$PWD/../../src/core $$PWD/../../src
QMAKE_CXXFLAGS += -isystem $$PWD/../../third_party/googletest/include
SOURCES += $$files($$PWD/*.cpp)

LIBS += $$OUT_PWD/../../src/hardware/libresonant_hardware.a $$OUT_PWD/../../src/core/libresonant_core.a \
    $$OUT_PWD/../../third_party/libresonant_gtest.a -lpthread
PRE_TARGETDEPS += $$OUT_PWD/../../src/hardware/libresonant_hardware.a $$OUT_PWD/../../src/core/libresonant_core.a \
    $$OUT_PWD/../../third_party/libresonant_gtest.a

# Lets the tests find the example projects.
DEFINES += RESONANT_SOURCE_DIR=\\\"$$clean_path($$PWD/../..)\\\"
