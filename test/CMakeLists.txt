include(GoogleTest)

foreach(test_name registry_test configuration_test concurrency_test allocation_test)
    add_executable(${test_name} ${test_name}.cpp)

    target_link_libraries(${test_name}
        PRIVATE
        cppfig::cppfig
        cppfig::testing
        cppfig_warnings
        GTest::gtest_main
    )

    gtest_discover_tests(${test_name})
    coverage_register_target(${test_name})
endforeach()

find_package(Threads REQUIRED)
target_link_libraries(concurrency_test PRIVATE Threads::Threads)

foreach(case_name invalid_default duplicate_setting unregistered_setting)
    set(test_name compile_fail_${case_name})
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_CXX_COMPILER}
            -std=c++20
            -I${PROJECT_SOURCE_DIR}/src
            -c ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${case_name}.cpp
            -o ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.o
    )
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
endforeach()
