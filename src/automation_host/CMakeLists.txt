# resonant-automation: the headless automation host (REQ-CHAT-20261126-012).
add_executable(resonant-automation main.cpp)
target_link_libraries(resonant-automation PRIVATE resonant_automation_api resonant_core resonant_warnings Threads::Threads)
install(TARGETS resonant-automation RUNTIME DESTINATION bin)
