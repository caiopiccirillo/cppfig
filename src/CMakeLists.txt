set(CPPFIG_CORE_HEADERS
    cppfig/cppfig.h
    cppfig/configuration.h
    cppfig/diff.h
    cppfig/conf.h
    cppfig/interface.h
    cppfig/logging.h
    cppfig/schema.h
    cppfig/serializer.h
    cppfig/setting.h
    cppfig/status.h
    cppfig/thread_policy.h
    cppfig/traits.h
    cppfig/validator.h
    cppfig/value.h
)

add_library(cppfig INTERFACE)
add_library(cppfig::cppfig ALIAS cppfig)

# FILE_SET carries both the include directory and the list of headers to
# install, so consumers get the same layout whether they add_subdirectory or
# find_package.
target_sources(cppfig
    INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    FILES ${CPPFIG_CORE_HEADERS} cppfig/json.h
)

target_compile_features(cppfig INTERFACE cxx_std_20)

# Propagate optional serializer flags to consumers.
if(CPPFIG_ENABLE_JSON)
    target_compile_definitions(cppfig INTERFACE CPPFIG_HAS_JSON)
    target_link_libraries(cppfig INTERFACE nlohmann_json::nlohmann_json)
endif()
# Future: if(CPPFIG_ENABLE_TOML) ... endif()

# testing/mock.h includes <gmock/gmock.h>, so it ships as its own target
# rather than making GMock a dependency of the library itself.
if(CPPFIG_BUILD_TESTS)
    add_library(cppfig_testing INTERFACE)
    add_library(cppfig::testing ALIAS cppfig_testing)

    target_sources(cppfig_testing
        INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
        FILES cppfig/testing/mock.h
    )

    target_link_libraries(cppfig_testing INTERFACE cppfig::cppfig GTest::gmock)

    # Every header must compile on its own. json.h is only includable when the
    # JSON back-end is enabled; testing/mock.h additionally needs GMock.
    set(_header_check_headers ${CPPFIG_CORE_HEADERS} cppfig/testing/mock.h)
    if(CPPFIG_ENABLE_JSON)
        list(APPEND _header_check_headers cppfig/json.h)
    endif()

    add_header_self_containment_target(cppfig_header_check
        LINK_LIBRARIES cppfig::cppfig cppfig::testing cppfig_warnings
        HEADERS ${_header_check_headers}
    )
endif()
