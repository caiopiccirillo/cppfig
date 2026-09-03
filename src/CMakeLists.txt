set(CPPFIG_CORE_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/cppfig.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/configuration.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/diff.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/conf.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/interface.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/logging.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/schema.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/serializer.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/setting.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/status.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/traits.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/validator.h
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/value.h
)

add_library(cppfig INTERFACE)

target_sources(cppfig
    INTERFACE
    ${CPPFIG_CORE_HEADERS}
    ${CMAKE_CURRENT_LIST_DIR}/cppfig/json.h
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

# Every header must compile on its own. json.h is only includable when the
# JSON back-end is enabled; testing/mock.h additionally needs GMock.
set(_header_check_headers ${CPPFIG_CORE_HEADERS} ${CMAKE_CURRENT_LIST_DIR}/cppfig/testing/mock.h)
if(CPPFIG_ENABLE_JSON)
    list(APPEND _header_check_headers ${CMAKE_CURRENT_LIST_DIR}/cppfig/json.h)
endif()

add_header_self_containment_target(cppfig_header_check
    BASE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LINK_LIBRARIES cppfig GTest::gmock
    HEADERS ${_header_check_headers}
)
