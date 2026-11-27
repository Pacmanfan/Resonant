# resonant_hardware: drivers for real instruments (ALSA, SoapySDR) behind the
# hardware abstraction of the core. No Qt. The optional libraries are found
# by hardware.pri (included through warnings.pri).
include(../../warnings.pri)

TEMPLATE = lib
TARGET = resonant_hardware
CONFIG += staticlib
CONFIG -= qt

INCLUDEPATH += $$PWD $$PWD/../core
SOURCES += $$files($$PWD/*.cpp)
HEADERS += $$files($$PWD/*.h)
PRE_TARGETDEPS += $$OUT_PWD/../core/libresonant_core.a
