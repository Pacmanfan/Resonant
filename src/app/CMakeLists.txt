# resonant_app: the desktop application's windows, panels and controllers
# as a library, so the tests can drive the real user interface.
file(GLOB RESONANT_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB RESONANT_APP_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB RESONANT_APP_FORMS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/forms/*.ui)

add_library(resonant_app STATIC ${RESONANT_APP_SOURCES} ${RESONANT_APP_HEADERS} ${RESONANT_APP_FORMS})
set_target_properties(resonant_app PROPERTIES AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/forms)
target_include_directories(resonant_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Development builds find demos, examples, plugins and the Python package in the source tree (ResourcePaths);
# a bundle never names the source tree (RES-DIST-003).
if(NOT RESONANT_BUNDLE_LAYOUT)
    target_compile_definitions(resonant_app PRIVATE RESONANT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()
target_link_libraries(resonant_app
    PUBLIC resonant_render resonant_hardware resonant_core Qt6::Widgets Qt6::OpenGLWidgets
    PRIVATE resonant_warnings)
# The automation server (the GUI hosts the local endpoint), when the API is built.
if(TARGET resonant_automation_api)
    target_link_libraries(resonant_app PUBLIC resonant_automation_api)
endif()

add_executable(resonant ${CMAKE_CURRENT_SOURCE_DIR}/../main/main.cpp)
target_link_libraries(resonant PRIVATE resonant_app resonant_warnings)
install(TARGETS resonant RUNTIME DESTINATION bin)
