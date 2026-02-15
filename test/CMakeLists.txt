enable_testing()

add_executable(
  unit_test
  unit_test.cpp
)

target_link_libraries(
  unit_test
  GTest::gtest_main
  GTest::gmock
  cppfig
)

add_executable(
  integration_test
  integration_test.cpp
)

target_link_libraries(
  integration_test
  GTest::gtest_main
  GTest::gmock
  cppfig
)

add_executable(
  thread_safety_test
  thread_safety_test.cpp
)

target_link_libraries(
  thread_safety_test
  GTest::gtest_main
  GTest::gmock
  cppfig
  pthread
)

include(GoogleTest)
gtest_discover_tests(unit_test)
gtest_discover_tests(integration_test)
gtest_discover_tests(thread_safety_test)
