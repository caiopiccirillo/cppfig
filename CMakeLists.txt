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

if(STRICT_BUILD)
    message(STATUS "Strict build enabled — adding comprehensive compiler warnings")

    # Warnings common to GCC and Clang
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror

        # Type-safety & conversions
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wfloat-equal
        -Wcast-align
        -Wcast-qual

        # C++ best practices
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnon-virtual-dtor
        -Wshadow
        -Wundef

        # Control-flow & logic
        -Wimplicit-fallthrough
        -Wswitch-enum
        -Wnull-dereference

        # Formatting & style
        -Wformat=2
        -Wmisleading-indentation
        -Wunused
        -Wredundant-decls
    )

    # GCC-only warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
            -Wsuggest-override
            -Wstringop-overflow=4
            -Warray-bounds=2
        )
    endif()

    # Clang-only warnings
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wextra-semi
            -Wnewline-eof
            -Wheader-hygiene
            -Wdocumentation
        )
    endif()
endif()

configure_file(CMakeConfig.h.in "${CMAKE_BINARY_DIR}/CMakeConfig.h")
include_directories(${CMAKE_BINARY_DIR})

# Uncomment to enable clang-tidy during compilation
# set(CMAKE_CXX_CLANG_TIDY clang-tidy;-fix;-fix-errors)

add_subdirectory(src)
add_subdirectory(examples)

add_subdirectory(test)
add_subdirectory(benchmark)
include(CTest)

# Add coverage targets if coverage is enabled
add_coverage_target()

include(CPackConfig.cmake)
include(CPack)
