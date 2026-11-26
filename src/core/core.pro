# resonant_core: the engineering data model and physics. No Qt, no OpenGL.
include(../../warnings.pri)

TEMPLATE = lib
TARGET = resonant_core
CONFIG += staticlib
CONFIG -= qt

INCLUDEPATH += $$PWD
SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)
