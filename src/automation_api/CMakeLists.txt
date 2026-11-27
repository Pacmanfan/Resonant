# resonant_automation_api: the protobuf adapter of the application services
# (REQ-CHAT-20261126-012, ADR-000216). The C++ code and the descriptor set of
# proto/ are generated into the build tree; they are never committed.
file(GLOB_RECURSE RESONANT_PROTOS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/proto/*.proto)
set(RESONANT_PROTO_ROOT ${CMAKE_SOURCE_DIR}/proto)
set(RESONANT_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(RESONANT_API_DESCRIPTORS ${CMAKE_CURRENT_BINARY_DIR}/resonant_api.desc)
file(MAKE_DIRECTORY ${RESONANT_GENERATED})
set(RESONANT_GENERATED_SOURCES)
set(RESONANT_GENERATED_HEADERS)
foreach(proto ${RESONANT_PROTOS})
    file(RELATIVE_PATH rel ${RESONANT_PROTO_ROOT} ${proto})
    string(REGEX REPLACE "\\.proto$" "" base ${rel})
    list(APPEND RESONANT_GENERATED_SOURCES ${RESONANT_GENERATED}/${base}.pb.cc)
    list(APPEND RESONANT_GENERATED_HEADERS ${RESONANT_GENERATED}/${base}.pb.h)
endforeach()
# One protoc run for everything: every header exists before anything compiles.
add_custom_command(
    OUTPUT ${RESONANT_GENERATED_SOURCES} ${RESONANT_GENERATED_HEADERS} ${RESONANT_API_DESCRIPTORS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I${RESONANT_PROTO_ROOT} --cpp_out=${RESONANT_GENERATED}
            --descriptor_set_out=${RESONANT_API_DESCRIPTORS} --include_imports --include_source_info ${RESONANT_PROTOS}
    DEPENDS ${RESONANT_PROTOS}
    COMMENT "Generating the resonant.v1 protobuf code"
    VERBATIM)

file(GLOB RESONANT_API_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(resonant_automation_api STATIC ${RESONANT_API_SOURCES} ${RESONANT_GENERATED_SOURCES})
target_include_directories(resonant_automation_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(resonant_automation_api SYSTEM PUBLIC ${RESONANT_GENERATED})
target_link_libraries(resonant_automation_api PUBLIC resonant_core protobuf::libprotobuf PRIVATE resonant_warnings)
target_compile_definitions(resonant_automation_api PUBLIC RESONANT_HAVE_AUTOMATION_API)
install(FILES ${RESONANT_API_DESCRIPTORS} DESTINATION share/resonant/automation)
