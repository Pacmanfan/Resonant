# Resonant - qmake build. The same targets are built by CMake (CMakeLists.txt).
# Each project collects its sources by globbing its directory, so re-run
# qmake after adding or removing a source file.
TEMPLATE = subdirs

SUBDIRS = gtest core hardware render app main help core_tests app_tests

# The Python API module, when Python and pybind11 are found (python.pri).
include(python.pri)
equals(RESONANT_PYTHON, 1) {
    SUBDIRS += python
    python.file = src/python/python.pro
    python.depends = core
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
