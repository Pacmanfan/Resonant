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

add_library(resonant_json INTERFACE)
target_include_directories(resonant_json SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann)

add_subdirectory(src/core)

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
