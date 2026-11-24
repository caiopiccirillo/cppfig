include(GoogleTest)

set(unit_tests registry_test configuration_test concurrency_test traits_test)
if(NOT CPPFIG_THREAD_SANITIZER)
    list(APPEND unit_tests allocation_test)
endif()

foreach(test_name ${unit_tests})
    add_executable(${test_name} ${test_name}.cpp)

    target_link_libraries(${test_name}
        PRIVATE
        cppfig::cppfig
        cppfig_warnings
        GTest::gtest_main
    )

    gtest_discover_tests(${test_name})
    coverage_register_target(${test_name})
endforeach()

find_package(Threads REQUIRED)
target_link_libraries(concurrency_test PRIVATE Threads::Threads)
target_link_libraries(configuration_test PRIVATE Threads::Threads)
if(TARGET allocation_test)
    target_include_directories(allocation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compile_scale)
endif()

# Each case must fail to compile, with its own diagnostic. The script checks
# both: matching the message keeps an unrelated error, such as a typo in the
# case itself, from passing the test, and requiring the failure keeps a matching
# warning from passing it.
set(compile_fail_invalid_default_regex "A constexpr-evaluable default value failed its setting validator")
set(compile_fail_duplicate_setting_regex "Registry paths must be unique")
set(compile_fail_unregistered_setting_regex "has_setting")
set(compile_fail_duplicate_env_regex "Environment override names must be unique")
set(compile_fail_malformed_env_regex "Environment override names must be non-empty")
set(compile_fail_get_on_temporary_regex "deleted")

foreach(case_name invalid_default duplicate_setting unregistered_setting duplicate_env malformed_env get_on_temporary)
    set(test_name compile_fail_${case_name})
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            -DCOMPILER=${CMAKE_CXX_COMPILER}
            -DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/src
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${case_name}.cpp
            -DOBJECT=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.o
            "-DEXPECTED=${${test_name}_regex}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/expect_compile_failure.cmake
    )
endforeach()

# The core headers must not include <filesystem>: it is opt-in through
# <cppfig/path.h> and <cppfig/file.h>.
add_test(
    NAME core_headers_do_not_include_filesystem
    COMMAND ${CMAKE_CXX_COMPILER}
        -std=c++20
        -I${PROJECT_SOURCE_DIR}/src
        -fsyntax-only ${CMAKE_CURRENT_SOURCE_DIR}/core_headers/no_filesystem.cpp
)

# Compile-time budget: the full initialization path for a 1024-setting registry
# must build at -O2 well within the timeout. Exclude with `ctest -LE compile-scale`.
add_test(
    NAME compile_scale_1024
    COMMAND ${CMAKE_CXX_COMPILER}
        -std=c++20
        -O2
        -Wall -Wextra -Wpedantic -Werror
        -DCPPFIG_SCALE_SETTINGS=1024
        -I${PROJECT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/compile_scale/large_registry.cpp
        -o ${CMAKE_CURRENT_BINARY_DIR}/compile_scale_1024.o
)
set_tests_properties(compile_scale_1024 PROPERTIES TIMEOUT 120 LABELS compile-scale)
