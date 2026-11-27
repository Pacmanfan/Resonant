# resonant_automation_api: the protobuf adapter of the application services
# (generated messages, the dispatcher, the local endpoint). The C++ code of
# proto/ is generated into this build directory when qmake runs and again when
# a .proto changes; it is never committed (ADR-000216).
include(../../warnings.pri)
include(../../protobuf.pri)

TEMPLATE = lib
TARGET = resonant_automation_api
CONFIG += staticlib
QT =

PROTO_ROOT = $$clean_path($$PWD/../../proto)
GENERATED = $$OUT_PWD/generated
PROTOS = $$files($$PROTO_ROOT/*.proto, true)

# Generate everything once now, so every header exists before the first compile.
!exists($$GENERATED): mkpath($$GENERATED)
system($$RESONANT_PROTOC -I$$PROTO_ROOT --cpp_out=$$GENERATED --descriptor_set_out=$$OUT_PWD/resonant_api.desc --include_imports --include_source_info $$PROTOS)

# qmake hands the rule each .proto relative to the build directory.
defineReplace(protoSource) {
    rel = $$relative_path($$absolute_path($$1, $$OUT_PWD), $$PROTO_ROOT)
    return($$GENERATED/$$replace(rel, \\.proto$, .pb.cc))
}
PROTO_ROOT_FROM_BUILD = $$relative_path($$PROTO_ROOT, $$OUT_PWD)
protoc.input = PROTOS
protoc.output_function = protoSource
protoc.commands = $$RESONANT_PROTOC -I$$PROTO_ROOT_FROM_BUILD --cpp_out=$$GENERATED ${QMAKE_FILE_IN}
protoc.variable_out = GENERATED_SOURCES
protoc.CONFIG += target_predeps
QMAKE_EXTRA_COMPILERS += protoc

# The descriptor set (the Python bridge and clients build their messages from it).
descriptors.target = $$OUT_PWD/resonant_api.desc
descriptors.depends = $$PROTOS
descriptors.commands = $$RESONANT_PROTOC -I$$PROTO_ROOT --descriptor_set_out=$$OUT_PWD/resonant_api.desc --include_imports --include_source_info $$PROTOS
QMAKE_EXTRA_TARGETS += descriptors
PRE_TARGETDEPS += $$OUT_PWD/resonant_api.desc

INCLUDEPATH += $$PWD $$PWD/../core
# Generated headers are treated like third-party headers (their warnings are not ours).
QMAKE_CXXFLAGS += -isystem $$GENERATED $$RESONANT_PROTOBUF_CFLAGS
SOURCES += $$files($$PWD/*.cpp)
HEADERS += $$files($$PWD/*.h)

isEmpty(PREFIX): PREFIX = /usr/local
descriptorfile.path = $$PREFIX/share/resonant/automation
descriptorfile.files = $$OUT_PWD/resonant_api.desc
descriptorfile.CONFIG += no_check_exist
INSTALLS += descriptorfile
