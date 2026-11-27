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
    $$OUT_PWD/../../src/hardware/libresonant_hardware.a $$OUT_PWD/../../src/core/libresonant_core.a $$OUT_PWD/../../third_party/libresonant_gtest.a
LIBS += $$RESONANT_LIBS
PRE_TARGETDEPS += $$RESONANT_LIBS

include(../../protobuf.pri)
equals(RESONANT_AUTOMATION_API, 1) {
    DEFINES += RESONANT_HAVE_AUTOMATION_API
    INCLUDEPATH += $$PWD/../../src/automation_api
    QMAKE_CXXFLAGS += -isystem $$OUT_PWD/../../src/automation_api/generated $$RESONANT_PROTOBUF_CFLAGS
    LIBS += $$OUT_PWD/../../src/automation_api/libresonant_automation_api.a $$OUT_PWD/../../src/core/libresonant_core.a $$RESONANT_PROTOBUF_LIBS
    PRE_TARGETDEPS += $$OUT_PWD/../../src/automation_api/libresonant_automation_api.a
}

# Lets the tests find the example projects.
DEFINES += RESONANT_SOURCE_DIR=\\\"$$clean_path($$PWD/../..)\\\"
