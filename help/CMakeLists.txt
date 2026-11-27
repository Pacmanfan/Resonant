# The in-app help and the shared data of an installation (see help/help.pro).
add_custom_target(resonant_help ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/help/build_help.py
            --source ${CMAKE_CURRENT_SOURCE_DIR}/src
            --screenshots ${CMAKE_CURRENT_SOURCE_DIR}/screenshots/manifest.json
            --out ${CMAKE_CURRENT_BINARY_DIR} --draft --quiet
    COMMENT "Building the in-app help"
    VERBATIM)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/resonant/help)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/index.json ${CMAKE_CURRENT_BINARY_DIR}/search.json ${CMAKE_CURRENT_BINARY_DIR}/build-report.json
        DESTINATION share/resonant/help)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/demos ${CMAKE_SOURCE_DIR}/examples DESTINATION share/resonant)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/plugins/examples DESTINATION share/resonant/plugins)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/python/resonant ${CMAKE_SOURCE_DIR}/python/resonant_api ${CMAKE_SOURCE_DIR}/python/examples ${CMAKE_SOURCE_DIR}/python/templates
        DESTINATION share/resonant/python PATTERN "__pycache__" EXCLUDE)
