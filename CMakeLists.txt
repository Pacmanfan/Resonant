cmake_minimum_required(VERSION 3.21)
project(Resonant VERSION 0.1.0 DESCRIPTION "Wave Field Engineering Studio" LANGUAGES CXX)

# The same targets are built by qmake (project.pro). Both builds collect the
# sources of a target by globbing its directory, so there is no source list
# to keep in step; after adding or removing a file, re-run the configure step
# (cmake does it automatically through CONFIGURE_DEPENDS).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(RESONANT_BUILD_APP "Build the rendering library and the desktop application (needs Qt 6)" ON)
option(RESONANT_BUILD_TESTS "Build the test suites" ON)
# Optional acceleration (ADR-000212): used when found, off on request.
option(RESONANT_WITH_VOLK "Use VOLK vector kernels when VOLK is installed" ON)
option(RESONANT_WITH_CUDA "Build the CUDA solver backend when a CUDA toolkit is found" ON)
# Optional measurement libraries (ADR-000213): used when found, off on request.
option(RESONANT_WITH_HDF5 "Store measurements in HDF5 when HDF5 is installed (else Resonant's binary format)" ON)
option(RESONANT_WITH_ALSA "Build the ALSA audio-interface driver when ALSA is installed" ON)
option(RESONANT_WITH_SOAPYSDR "Build the SoapySDR driver (discovery only) when SoapySDR is installed" ON)
set(RESONANT_CUDA_ARCHITECTURES "75-real;86-real;89-real;120" CACHE STRING
    "GPU architectures compiled in (Turing, Ampere, Ada, Blackwell; the last also as PTX)")

# Warnings for Resonant's own code (third-party code is built without them).
add_library(resonant_warnings INTERFACE)
if(MSVC)
    target_compile_options(resonant_warnings INTERFACE /W4)
else()
    target_compile_options(resonant_warnings INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor>)
endif()

find_package(Eigen3 3.4 REQUIRED NO_MODULE)
find_package(Threads REQUIRED)

set(RESONANT_HAVE_VOLK OFF)
if(RESONANT_WITH_VOLK)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(VOLK QUIET IMPORTED_TARGET volk)
        if(VOLK_FOUND)
            set(RESONANT_HAVE_VOLK ON)
        endif()
    endif()
endif()
set(RESONANT_HAVE_CUDA OFF)
if(RESONANT_WITH_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        set(CMAKE_CUDA_ARCHITECTURES ${RESONANT_CUDA_ARCHITECTURES})
        set(CMAKE_CUDA_STANDARD 20)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        set(RESONANT_HAVE_CUDA ON)
    endif()
endif()
message(STATUS "Resonant acceleration: VOLK ${RESONANT_HAVE_VOLK}, CUDA ${RESONANT_HAVE_CUDA}")

find_package(PkgConfig QUIET)
set(RESONANT_HAVE_HDF5 OFF)
set(RESONANT_HAVE_ALSA OFF)
set(RESONANT_HAVE_SOAPYSDR OFF)
if(PkgConfig_FOUND)
    if(RESONANT_WITH_HDF5)
        pkg_check_modules(RESONANT_HDF5 QUIET IMPORTED_TARGET hdf5)
        if(RESONANT_HDF5_FOUND)
            set(RESONANT_HAVE_HDF5 ON)
        endif()
    endif()
    if(RESONANT_WITH_ALSA)
        pkg_check_modules(RESONANT_ALSA QUIET IMPORTED_TARGET alsa)
        if(RESONANT_ALSA_FOUND)
            set(RESONANT_HAVE_ALSA ON)
        endif()
    endif()
    if(RESONANT_WITH_SOAPYSDR)
        pkg_check_modules(RESONANT_SOAPYSDR QUIET IMPORTED_TARGET SoapySDR)
        if(RESONANT_SOAPYSDR_FOUND)
            set(RESONANT_HAVE_SOAPYSDR ON)
        endif()
    endif()
endif()
message(STATUS "Resonant measurement libraries: HDF5 ${RESONANT_HAVE_HDF5}, ALSA ${RESONANT_HAVE_ALSA}, SoapySDR ${RESONANT_HAVE_SOAPYSDR}")

add_library(resonant_json INTERFACE)
target_include_directories(resonant_json SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann)

add_subdirectory(src/core)
add_subdirectory(src/hardware)

if(RESONANT_BUILD_APP)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
    set(CMAKE_AUTOMOC ON)
    # Qt 6.4's moc cannot parse the C++20 concept syntax in libstdc++ 13
    # headers. Without the compiler's predefined macros moc sees those
    # headers as pre-C++20 and skips that code; moc only needs our own
    # class declarations.
    set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    add_subdirectory(src/render)
    add_subdirectory(src/app)
endif()

if(RESONANT_BUILD_TESTS)
    enable_testing()
    add_library(resonant_gtest STATIC third_party/googletest/src/gtest-all.cc)
    target_include_directories(resonant_gtest SYSTEM PUBLIC third_party/googletest/include)
    target_include_directories(resonant_gtest PRIVATE third_party/googletest)
    target_link_libraries(resonant_gtest PUBLIC Threads::Threads)
    add_subdirectory(tests/core)
    if(RESONANT_BUILD_APP)
        add_subdirectory(tests/app)
    endif()
endif()
