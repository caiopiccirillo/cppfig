cmake_minimum_required(VERSION 3.22.0)
project(cpp-template VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)

option(STRICT_BUILD "Enable warnings as errors" ON)

configure_file(CMakeConfig.h.in "${CMAKE_BINARY_DIR}/CMakeConfig.h")
include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(src)

add_subdirectory(test)
include(CTest)

include(CPackConfig.cmake)
include(CPack)
