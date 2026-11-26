# Tests of the rendering library and the application. They need a display
# with OpenGL 4.5 Core; without one, run them under xvfb-run.
include(../../warnings.pri)

TEMPLATE = app
TARGET = resonant_app_tests
CONFIG += console
CONFIG -= app_bundle
QT = core gui widgets opengl openglwidgets testlib

INCLUDEPATH += $$PWD/../../src/app $$PWD/../../src $$PWD/../../src/core
# The application's generated ui_*.h headers.
INCLUDEPATH += $$OUT_PWD/../../src/app
QMAKE_CXXFLAGS += -isystem $$PWD/../../third_party/googletest/include
SOURCES += $$files($$PWD/*.cpp)

RESONANT_LIBS = $$OUT_PWD/../../src/app/libresonant_app.a $$OUT_PWD/../../src/render/libresonant_render.a \
    $$OUT_PWD/../../src/core/libresonant_core.a $$OUT_PWD/../../third_party/libresonant_gtest.a
LIBS += $$RESONANT_LIBS
PRE_TARGETDEPS += $$RESONANT_LIBS
