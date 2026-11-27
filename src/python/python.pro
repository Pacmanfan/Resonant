# resonant._resonant: the native part of the Python API (ADR-000215). A
# Python extension module linking the static core; built only when
# python.pri finds Python and pybind11.
include(../../warnings.pri)
include(../../python.pri)

TEMPLATE = lib
CONFIG += plugin no_plugin_name_prefix shared
CONFIG -= qt
TARGET = _resonant

INCLUDEPATH += $$PWD/../core
QMAKE_CXXFLAGS += -isystem $$PYBIND11_INCLUDE $$replace(PYTHON_INCLUDES, -I, -isystem ) -fvisibility=hidden
# _resonant.cpython-312-x86_64-linux-gnu.so
QMAKE_EXTENSION_SHLIB = $$replace(PYTHON_EXT_SUFFIX, ^\\., )
SOURCES += $$PWD/ResonantModule.cpp

LIBS += $$OUT_PWD/../core/libresonant_core.a -lpthread
PRE_TARGETDEPS += $$OUT_PWD/../core/libresonant_core.a
# In-process dispatch of the automation API (the same contract as gRPC), when it is built.
include(../../protobuf.pri)
equals(RESONANT_AUTOMATION_API, 1): useAutomationApi($$OUT_PWD/../automation_api)

# `make install`: the module into the installed package (share/resonant/python/resonant).
isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/share/resonant/python/resonant
INSTALLS += target
