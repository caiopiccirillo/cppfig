add_library(cppfig INTERFACE)

target_sources(cppfig
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cppfig.h
    ${CMAKE_CURRENT_LIST_DIR}/ConfigurationTraits.h
    ${CMAKE_CURRENT_LIST_DIR}/ConfigHelpers.h
    ${CMAKE_CURRENT_LIST_DIR}/CommonConfigurations.h
    ${CMAKE_CURRENT_LIST_DIR}/GenericConfiguration.h
    ${CMAKE_CURRENT_LIST_DIR}/IConfiguration.h
    ${CMAKE_CURRENT_LIST_DIR}/IConfigurationSerializer.h
    ${CMAKE_CURRENT_LIST_DIR}/JsonSerializer.h
    ${CMAKE_CURRENT_LIST_DIR}/Setting.h
    ${CMAKE_CURRENT_LIST_DIR}/TypedSetting.h
)

target_include_directories(cppfig
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(cppfig
    INTERFACE
    nlohmann_json::nlohmann_json
)
