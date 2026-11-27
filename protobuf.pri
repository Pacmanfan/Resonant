# The automation API (REQ-CHAT-20261126-012, ADR-000216): Protocol Buffers
# for the public contract in proto/. Built when protoc and the protobuf
# library are found; CONFIG+=no_automation_api builds without it. gRPC is not
# linked into the application: its transport is a separate bridge process.
RESONANT_AUTOMATION_API = 0
!no_automation_api {
    RESONANT_PROTOC = $$system(command -v protoc 2>/dev/null)
    RESONANT_PROTOBUF_LIBS = $$system(pkg-config --libs protobuf 2>/dev/null)
    !isEmpty(RESONANT_PROTOC):!isEmpty(RESONANT_PROTOBUF_LIBS) {
        RESONANT_AUTOMATION_API = 1
        RESONANT_PROTOBUF_CFLAGS = $$system(pkg-config --cflags protobuf 2>/dev/null)
    }
}

# useAutomationApi(<build directory of src/automation_api>): compile and link
# against the adapter (it goes before the libraries already listed, which
# include the core it uses).
RESONANT_AUTOMATION_API_SOURCE = $$PWD/src/automation_api  # $$PWD inside a function is the caller's
defineTest(useAutomationApi) {
    api_out = $$1
    DEFINES += RESONANT_HAVE_AUTOMATION_API
    INCLUDEPATH += $$RESONANT_AUTOMATION_API_SOURCE
    QMAKE_CXXFLAGS += -isystem $$api_out/generated $$RESONANT_PROTOBUF_CFLAGS
    LIBS = $$api_out/libresonant_automation_api.a $$LIBS $$RESONANT_PROTOBUF_LIBS
    PRE_TARGETDEPS += $$api_out/libresonant_automation_api.a
    export(DEFINES)
    export(INCLUDEPATH)
    export(QMAKE_CXXFLAGS)
    export(LIBS)
    export(PRE_TARGETDEPS)
    return(true)
}
