# resonant._resonant: the native part of the Python API (ADR-000215).
pybind11_add_module(_resonant MODULE ResonantModule.cpp)
target_link_libraries(_resonant PRIVATE resonant_core resonant_warnings)
install(TARGETS _resonant LIBRARY DESTINATION share/resonant/python/resonant)
