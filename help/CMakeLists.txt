# The in-app help and the shared data of an installation (see help/help.pro).
# A bundle (the Linux ZIP) builds the release help: a missing screenshot fails it.
set(RESONANT_HELP_MODE --draft)
if(RESONANT_BUNDLE_LAYOUT OR NOT RESONANT_HELP_DRAFT)
    set(RESONANT_HELP_MODE)
endif()
add_custom_target(resonant_help ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/help/build_help.py
            --source ${CMAKE_CURRENT_SOURCE_DIR}/src
            --screenshots ${CMAKE_CURRENT_SOURCE_DIR}/screenshots/manifest.json
            --out ${CMAKE_CURRENT_BINARY_DIR} ${RESONANT_HELP_MODE} --quiet
    COMMENT "Building the in-app help"
    VERBATIM)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION ${RESONANT_INSTALL_DATADIR}/help)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/index.html ${CMAKE_CURRENT_BINARY_DIR}/index.json ${CMAKE_CURRENT_BINARY_DIR}/search.json
              ${CMAKE_CURRENT_BINARY_DIR}/build-report.json
        DESTINATION ${RESONANT_INSTALL_DATADIR}/help)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/demos ${CMAKE_SOURCE_DIR}/examples DESTINATION ${RESONANT_INSTALL_DATADIR} PATTERN "__pycache__" EXCLUDE)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/plugins/examples DESTINATION ${RESONANT_INSTALL_DATADIR}/plugins PATTERN "__pycache__" EXCLUDE)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/python/resonant ${CMAKE_SOURCE_DIR}/python/resonant_api ${CMAKE_SOURCE_DIR}/python/examples ${CMAKE_SOURCE_DIR}/python/templates
        DESTINATION ${RESONANT_INSTALL_DATADIR}/python PATTERN "__pycache__" EXCLUDE)
