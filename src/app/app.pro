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
