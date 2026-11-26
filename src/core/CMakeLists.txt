# resonant_core: the engineering data model and physics. No Qt, no OpenGL.
file(GLOB_RECURSE RESONANT_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE RESONANT_CORE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(resonant_core STATIC ${RESONANT_CORE_SOURCES} ${RESONANT_CORE_HEADERS})
target_include_directories(resonant_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(resonant_core PUBLIC Eigen3::Eigen resonant_json Threads::Threads PRIVATE resonant_warnings)
