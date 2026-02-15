cmake_minimum_required(VERSION 3.22.0)
project(cppfig VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompileCommands)
include(Coverage)

# Enable copying of compile_commands.json to project root
copy_compile_commands()

find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

option(STRICT_BUILD "Enable warnings as errors" ON)

configure_file(CMakeConfig.h.in "${CMAKE_BINARY_DIR}/CMakeConfig.h")
include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(examples)

add_subdirectory(test)
add_subdirectory(benchmark)
include(CTest)

# Add coverage targets if coverage is enabled
add_coverage_target()

include(CPackConfig.cmake)
include(CPack)
