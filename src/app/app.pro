# resonant_app: the desktop application's windows, panels and controllers
# as a library, so the tests can drive the real user interface.
include(../../warnings.pri)

TEMPLATE = lib
TARGET = resonant_app
CONFIG += staticlib
QT = core gui widgets opengl openglwidgets

INCLUDEPATH += $$PWD $$PWD/.. $$PWD/../core
SOURCES += $$files($$PWD/*.cpp)
HEADERS += $$files($$PWD/*.h)
FORMS += $$files($$PWD/forms/*.ui)
PRE_TARGETDEPS += $$OUT_PWD/../core/libresonant_core.a $$OUT_PWD/../hardware/libresonant_hardware.a $$OUT_PWD/../render/libresonant_render.a
# The automation server (the GUI hosts the local endpoint), when the API is built.
include(../../protobuf.pri)
equals(RESONANT_AUTOMATION_API, 1) {
    DEFINES += RESONANT_HAVE_AUTOMATION_API
    INCLUDEPATH += $$PWD/../automation_api
    QMAKE_CXXFLAGS += -isystem $$OUT_PWD/../automation_api/generated $$RESONANT_PROTOBUF_CFLAGS
    PRE_TARGETDEPS += $$OUT_PWD/../automation_api/libresonant_automation_api.a
}
# Development builds find the demonstrations, examples, plugins and Python
# package in the source tree (ResourcePaths); installations use share/resonant.
DEFINES += RESONANT_SOURCE_DIR=\\\"$$clean_path($$PWD/../..)\\\"
