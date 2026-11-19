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
    cppfig/testing/mock.h
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

# The test fake (cppfig/testing/mock.h) does not depend on any test framework,
# so it ships and installs with the library for consumers' own tests.
if(CPPFIG_BUILD_TESTS)
    add_header_self_containment_target(cppfig_header_check
        LINK_LIBRARIES cppfig::cppfig cppfig_warnings
        HEADERS ${CPPFIG_CORE_HEADERS}
    )
endif()
