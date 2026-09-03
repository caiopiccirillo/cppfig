include(GoogleTest)

foreach(test_name unit_test integration_test thread_safety_test)
    add_executable(${test_name} ${test_name}.cpp)

    target_link_libraries(${test_name}
        PRIVATE
        cppfig::cppfig
        cppfig::testing
        cppfig_warnings
        GTest::gtest_main
        GTest::gmock
    )

    gtest_discover_tests(${test_name})

    # Register test targets for LLVM source-based coverage reporting
    coverage_register_target(${test_name})
endforeach()

find_package(Threads REQUIRED)
target_link_libraries(thread_safety_test PRIVATE Threads::Threads)
