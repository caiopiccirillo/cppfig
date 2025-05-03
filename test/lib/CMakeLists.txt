add_executable(GreeterTest)

target_sources(GreeterTest
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/GreeterTest.cpp
    )

target_link_libraries(GreeterTest 
                      GreeterLib
                      GTest::gmock
                      GTest::gmock_main
                      GTest::gtest
                      GTest::gtest_main)
