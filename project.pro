# Resonant - qmake build. The same targets are built by CMake (CMakeLists.txt).
# Each project collects its sources by globbing its directory, so re-run
# qmake after adding or removing a source file.
TEMPLATE = subdirs

SUBDIRS = gtest core hardware render app main core_tests app_tests

gtest.file = third_party/gt_library.pro
core.file = src/core/core.pro
hardware.file = src/hardware/hardware.pro
render.file = src/render/render.pro
app.file = src/app/app.pro
main.file = src/main/main.pro
core_tests.file = tests/core/core_tests.pro
app_tests.file = tests/app/app_tests.pro

hardware.depends = core
render.depends = core
app.depends = core hardware render
main.depends = core hardware render app
core_tests.depends = core hardware gtest
app_tests.depends = core hardware render app gtest
