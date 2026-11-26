# Tests of the rendering library and the application. They need a display
# with OpenGL 4.5 Core; without one, run them under xvfb-run.
file(GLOB RESONANT_APP_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(resonant_app_tests ${RESONANT_APP_TEST_SOURCES})
set_target_properties(resonant_app_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(resonant_app_tests PRIVATE resonant_app resonant_gtest Qt6::Test resonant_warnings)

find_program(RESONANT_XVFB_RUN xvfb-run)
if(RESONANT_XVFB_RUN)
    add_test(NAME resonant_app_tests COMMAND ${RESONANT_XVFB_RUN} -a -s "-screen 0 1600x1000x24" $<TARGET_FILE:resonant_app_tests>)
else()
    add_test(NAME resonant_app_tests COMMAND resonant_app_tests)
endif()
