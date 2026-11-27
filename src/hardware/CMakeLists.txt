# resonant_hardware: drivers for real instruments (ALSA, SoapySDR) behind the
# hardware abstraction of the core. No Qt.
file(GLOB RESONANT_HARDWARE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB RESONANT_HARDWARE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(resonant_hardware STATIC ${RESONANT_HARDWARE_SOURCES} ${RESONANT_HARDWARE_HEADERS})
target_include_directories(resonant_hardware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(resonant_hardware PUBLIC resonant_core PRIVATE resonant_warnings)
if(RESONANT_HAVE_ALSA)
    target_compile_definitions(resonant_hardware PRIVATE RESONANT_HAVE_ALSA)
    target_link_libraries(resonant_hardware PRIVATE PkgConfig::RESONANT_ALSA)
endif()
if(RESONANT_HAVE_SOAPYSDR)
    target_compile_definitions(resonant_hardware PRIVATE RESONANT_HAVE_SOAPYSDR)
    target_link_libraries(resonant_hardware PRIVATE PkgConfig::RESONANT_SOAPYSDR)
endif()
