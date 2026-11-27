# resonant._resonant: the native part of the Python API (ADR-000215).
pybind11_add_module(_resonant MODULE ResonantModule.cpp)
target_link_libraries(_resonant PRIVATE resonant_core resonant_warnings)
# In-process dispatch of the automation API, when it is built.
if(TARGET resonant_automation_api)
    target_link_libraries(_resonant PRIVATE resonant_automation_api)
endif()
install(TARGETS _resonant LIBRARY DESTINATION ${RESONANT_INSTALL_DATADIR}/python/resonant)
