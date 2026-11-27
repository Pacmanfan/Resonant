# resonant: the desktop application executable.
include(../../warnings.pri)

TEMPLATE = app
TARGET = resonant
QT = core gui widgets opengl openglwidgets

INCLUDEPATH += $$PWD/../app $$PWD/.. $$PWD/../core
SOURCES += $$PWD/main.cpp

LIBS += $$OUT_PWD/../app/libresonant_app.a $$OUT_PWD/../render/libresonant_render.a $$OUT_PWD/../hardware/libresonant_hardware.a \
    $$OUT_PWD/../core/libresonant_core.a
PRE_TARGETDEPS += $$OUT_PWD/../app/libresonant_app.a $$OUT_PWD/../render/libresonant_render.a \
    $$OUT_PWD/../hardware/libresonant_hardware.a $$OUT_PWD/../core/libresonant_core.a

include(../../protobuf.pri)
equals(RESONANT_AUTOMATION_API, 1) {
    # The adapter, then the core again for what it uses (static libraries resolve in order).
    LIBS += $$OUT_PWD/../automation_api/libresonant_automation_api.a $$OUT_PWD/../core/libresonant_core.a $$RESONANT_PROTOBUF_LIBS
    PRE_TARGETDEPS += $$OUT_PWD/../automation_api/libresonant_automation_api.a
}

# `make install`: the executable to $$PREFIX/bin; help/help.pro installs the
# help, demonstrations, examples, plugins and Python package beside it.
isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
INSTALLS += target
