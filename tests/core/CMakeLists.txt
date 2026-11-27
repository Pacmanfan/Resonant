# Tests of the core library. They need no display and no OpenGL context.
file(GLOB RESONANT_CORE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(resonant_core_tests ${RESONANT_CORE_TEST_SOURCES})
target_link_libraries(resonant_core_tests PRIVATE resonant_hardware resonant_core resonant_gtest resonant_warnings)
add_test(NAME resonant_core_tests COMMAND resonant_core_tests)
target_compile_definitions(resonant_core_tests PRIVATE RESONANT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(TARGET resonant_automation_api)
    target_link_libraries(resonant_core_tests PRIVATE resonant_automation_api)
endif()
