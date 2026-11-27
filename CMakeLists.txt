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
option(RESONANT_WITH_PYTHON "Build the Python API module when Python and pybind11 are installed" ON)
option(RESONANT_WITH_AUTOMATION_API "Build the automation API (Protocol Buffers) when protoc and libprotobuf are installed" ON)
# The Linux ZIP (ADR-000218, tools/release/build_linux_zip.sh): data at the
# top of the package instead of share/resonant, the release help, no source or
# build paths compiled in, and the package_linux_zip target.
option(RESONANT_BUNDLE_LAYOUT "Install as the self-contained Linux bundle (a Release build only)" OFF)
option(RESONANT_HELP_DRAFT "Build the help in draft mode: a missing screenshot is a warning (never in a bundle)" ON)
if(RESONANT_BUNDLE_LAYOUT)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "RESONANT_BUNDLE_LAYOUT packages a Release build: configure with -DCMAKE_BUILD_TYPE=Release")
    endif()
    set(RESONANT_INSTALL_DATADIR ".")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=>"
                        "$<$<COMPILE_LANGUAGE:CXX>:-ffile-prefix-map=${CMAKE_BINARY_DIR}/=>")
else()
    set(RESONANT_INSTALL_DATADIR "share/resonant")
endif()
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

# The Python API module (ADR-000215): needs position-independent code in the
# static core it links.
set(RESONANT_HAVE_PYTHON OFF)
if(RESONANT_WITH_PYTHON)
    find_package(Python3 QUIET COMPONENTS Interpreter Development.Module)
    find_package(pybind11 CONFIG QUIET)
    if(Python3_FOUND AND pybind11_FOUND)
        set(RESONANT_HAVE_PYTHON ON)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
endif()
message(STATUS "Resonant Python API module: ${RESONANT_HAVE_PYTHON}")

# The automation API: the protobuf contract (proto/), its adapter and the
# headless host. gRPC itself is a separate bridge process (ADR-000216).
set(RESONANT_HAVE_AUTOMATION_API OFF)
if(RESONANT_WITH_AUTOMATION_API)
    find_package(Protobuf QUIET)
    if(Protobuf_FOUND AND Protobuf_PROTOC_EXECUTABLE)
        set(RESONANT_HAVE_AUTOMATION_API ON)
    endif()
endif()
message(STATUS "Resonant automation API: ${RESONANT_HAVE_AUTOMATION_API}")

add_library(resonant_json INTERFACE)
target_include_directories(resonant_json SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann)

add_subdirectory(src/core)
add_subdirectory(src/hardware)
if(RESONANT_HAVE_AUTOMATION_API)
    add_subdirectory(src/automation_api)
    add_subdirectory(src/automation_host)
endif()
if(RESONANT_HAVE_PYTHON)
    add_subdirectory(src/python)
endif()

# The in-app help needs only the Python interpreter (tools/help/build_help.py).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory(help)

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

# What the bundler records about this build (tools/release/package.py, RES-DIST-014).
if(RESONANT_BUNDLE_LAYOUT)
    if(RESONANT_HAVE_CUDA)
        string(APPEND CMAKE_CUDA_FLAGS " -Xcompiler=-ffile-prefix-map=${CMAKE_SOURCE_DIR}/= -Xcompiler=-ffile-prefix-map=${CMAKE_BINARY_DIR}/=")
    endif()
    set(RESONANT_BUILD_INFO "{\n  \"format\": \"resonant.build-info\", \"version\": 1,\n  \"applicationVersion\": \"${PROJECT_VERSION}\",\n  \"buildType\": \"${CMAKE_BUILD_TYPE}\",\n")
    string(APPEND RESONANT_BUILD_INFO "  \"compiler\": \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\",\n  \"cmake\": \"${CMAKE_VERSION}\",\n  \"generator\": \"${CMAKE_GENERATOR}\",\n")
    string(APPEND RESONANT_BUILD_INFO "  \"qt\": \"${Qt6_VERSION}\",\n  \"python\": \"${Python3_VERSION}\",\n  \"pythonExecutable\": \"${Python3_EXECUTABLE}\",\n")
    string(APPEND RESONANT_BUILD_INFO "  \"protobuf\": \"${Protobuf_VERSION}\",\n  \"cuda\": \"${CMAKE_CUDA_COMPILER_VERSION}\",\n")
    string(APPEND RESONANT_BUILD_INFO "  \"features\": {\"volk\": \"${RESONANT_HAVE_VOLK}\", \"cuda\": \"${RESONANT_HAVE_CUDA}\", \"hdf5\": \"${RESONANT_HAVE_HDF5}\", \"alsa\": \"${RESONANT_HAVE_ALSA}\", \"soapysdr\": \"${RESONANT_HAVE_SOAPYSDR}\", \"python\": \"${RESONANT_HAVE_PYTHON}\", \"automationApi\": \"${RESONANT_HAVE_AUTOMATION_API}\"}\n}\n")
    file(WRITE ${CMAKE_BINARY_DIR}/resonant-build.json "${RESONANT_BUILD_INFO}")
    # cmake --build <tree> --target package_linux_zip: install, bundle, validate, zip (dist/).
    add_custom_target(package_linux_zip
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/release/package.py bundle --build-dir ${CMAKE_BINARY_DIR} --out ${CMAKE_SOURCE_DIR}/dist
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Packaging the Linux ZIP"
        USES_TERMINAL VERBATIM)
    foreach(target resonant resonant-automation _resonant resonant_help)
        if(TARGET ${target})
            add_dependencies(package_linux_zip ${target})
        endif()
    endforeach()
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
