add_library(cppfig INTERFACE)

target_sources(cppfig
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/cppfig.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/configuration.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/diff.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/conf.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/interface.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/json.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/logging.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/schema.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/serializer.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/setting.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/status.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/traits.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/validator.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/value.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/testing/mock.h
)

target_include_directories(cppfig
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Propagate optional serializer flags to consumers.
if(CPPFIG_ENABLE_JSON)
    target_compile_definitions(cppfig INTERFACE CPPFIG_HAS_JSON)
    target_link_libraries(cppfig INTERFACE nlohmann_json::nlohmann_json)
endif()
# Future: if(CPPFIG_ENABLE_TOML) ... endif()
