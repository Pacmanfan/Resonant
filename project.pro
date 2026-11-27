# Resonant - qmake build. The same targets are built by CMake (CMakeLists.txt).
# Each project collects its sources by globbing its directory, so re-run
# qmake after adding or removing a source file.
TEMPLATE = subdirs

SUBDIRS = gtest core hardware render app main help core_tests app_tests

# The automation API (Protocol Buffers adapter, local endpoint, headless host),
# when protoc and libprotobuf are found (protobuf.pri).
include(protobuf.pri)
equals(RESONANT_AUTOMATION_API, 1) {
    SUBDIRS += automation_api automation_host
    automation_api.file = src/automation_api/automation_api.pro
    automation_api.depends = core
    automation_host.file = src/automation_host/automation_host.pro
    automation_host.depends = core automation_api
    app.depends += automation_api
    main.depends += automation_api
    core_tests.depends += automation_api
    app_tests.depends += automation_api
}

# The Python API module, when Python and pybind11 are found (python.pri).
include(python.pri)
equals(RESONANT_PYTHON, 1) {
    SUBDIRS += python
    python.file = src/python/python.pro
    python.depends = core
    equals(RESONANT_AUTOMATION_API, 1): python.depends += automation_api
}

gtest.file = third_party/gt_library.pro
core.file = src/core/core.pro
hardware.file = src/hardware/hardware.pro
render.file = src/render/render.pro
app.file = src/app/app.pro
main.file = src/main/main.pro
core_tests.file = tests/core/core_tests.pro
app_tests.file = tests/app/app_tests.pro
# The in-app help (HTML, navigation, search index) and the installed data.
help.file = help/help.pro

hardware.depends = core
render.depends = core
app.depends = core hardware render
main.depends = core hardware render app
core_tests.depends = core hardware gtest help
app_tests.depends = core hardware render app gtest help
