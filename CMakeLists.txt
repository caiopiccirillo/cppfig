cmake_minimum_required(VERSION 3.22.0)
project(cpp-template VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

find_package(GTest REQUIRED)

add_subdirectory(src)

add_subdirectory(test)
include(CTest)

include(CPackConfig.cmake)
include(CPack)
