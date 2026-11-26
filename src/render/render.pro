# resonant_render: OpenGL 4.5 Core rendering components. Depends on the
# core library and on Qt Gui / OpenGL, never on the application.
include(../../warnings.pri)

TEMPLATE = lib
TARGET = resonant_render
CONFIG += staticlib
QT = core gui opengl

INCLUDEPATH += $$PWD/.. $$PWD/../core
SOURCES += $$files($$PWD/*.cpp)
HEADERS += $$files($$PWD/*.h)
PRE_TARGETDEPS += $$OUT_PWD/../core/libresonant_core.a
