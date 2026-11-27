# resonant-automation: the headless automation host (REQ-CHAT-20261126-012).
# One project, the application services and the local endpoint, without a
# window; the gRPC bridge (python -m resonant_api.bridge) connects to it.
include(../../warnings.pri)
include(../../protobuf.pri)

TEMPLATE = app
TARGET = resonant-automation
CONFIG += console
CONFIG -= qt app_bundle

INCLUDEPATH += $$PWD/../core
SOURCES += $$PWD/main.cpp
LIBS += $$OUT_PWD/../core/libresonant_core.a -lpthread
PRE_TARGETDEPS += $$OUT_PWD/../core/libresonant_core.a
useAutomationApi($$OUT_PWD/../automation_api)

isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
INSTALLS += target
