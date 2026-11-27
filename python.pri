# The Python API module (resonant._resonant, ADR-000215): built when the
# Python development headers and pybind11 are found. Turn it off with qmake
# CONFIG+=no_python; the application never needs it. CMake: RESONANT_WITH_PYTHON.
RESONANT_PYTHON = 0
!no_python {
    PYTHON_INCLUDES = $$system(python3-config --includes 2>/dev/null)
    PYTHON_EXT_SUFFIX = $$system(python3-config --extension-suffix 2>/dev/null)
    !isEmpty(PYTHON_INCLUDES):!isEmpty(PYTHON_EXT_SUFFIX):exists(/usr/include/pybind11/pybind11.h) {
        RESONANT_PYTHON = 1
        PYBIND11_INCLUDE = /usr/include/pybind11
    }
}
