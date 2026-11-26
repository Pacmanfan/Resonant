# resonant_render: OpenGL 4.5 Core rendering components. Depends on the
# core library and on Qt Gui / OpenGL, never on the application.
file(GLOB RESONANT_RENDER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB RESONANT_RENDER_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(resonant_render STATIC ${RESONANT_RENDER_SOURCES} ${RESONANT_RENDER_HEADERS})
set_target_properties(resonant_render PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(resonant_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(resonant_render PUBLIC resonant_core Qt6::Gui Qt6::OpenGL PRIVATE resonant_warnings)
