set(CPPFIG_CORE_HEADERS
    cppfig/cppfig.h
    cppfig/chrono.h
    cppfig/conf.h
    cppfig/configuration.h
    cppfig/document.h
    cppfig/interface.h
    cppfig/schema.h
    cppfig/setting.h
    cppfig/status.h
    cppfig/storage.h
    cppfig/traits.h
    cppfig/validator.h
)

add_library(cppfig INTERFACE)
add_library(cppfig::cppfig ALIAS cppfig)

target_sources(cppfig
    INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    FILES ${CPPFIG_CORE_HEADERS}
)

target_compile_features(cppfig INTERFACE cxx_std_20)

# Test fakes are independent of GMock and ship as a separate opt-in target.
if(CPPFIG_BUILD_TESTS)
    add_library(cppfig_testing INTERFACE)
    add_library(cppfig::testing ALIAS cppfig_testing)

    target_sources(cppfig_testing
        INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
        FILES cppfig/testing/mock.h
    )

    target_link_libraries(cppfig_testing INTERFACE cppfig::cppfig)

    add_header_self_containment_target(cppfig_header_check
        LINK_LIBRARIES cppfig::cppfig cppfig::testing cppfig_warnings
        HEADERS ${CPPFIG_CORE_HEADERS} cppfig/testing/mock.h
    )
endif()
