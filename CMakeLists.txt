cmake_minimum_required(VERSION 3.31.0)
project(cppfig VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# cppfig uses no C++20 modules. Scanning for them adds a build step per source
# and puts @<file>.modmap arguments in compile_commands.json that only exist
# after a build, which breaks clang-tidy run on a configure-only tree.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Everything past the library itself exists to develop cppfig, not to consume
# it: a project that does add_subdirectory(cppfig) must not be made to supply
# GTest and Google Benchmark.
option(CPPFIG_BUILD_TESTS "Build the cppfig test suite" ${PROJECT_IS_TOP_LEVEL})
option(CPPFIG_BUILD_BENCHMARKS "Build the cppfig benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(CPPFIG_BUILD_EXAMPLES "Build the cppfig examples" ${PROJECT_IS_TOP_LEVEL})
option(CPPFIG_INSTALL "Generate the cppfig install and package rules" ${PROJECT_IS_TOP_LEVEL})
option(CPPFIG_BUILD_FUZZERS "Build the libFuzzer targets (Clang only)" OFF)

# ThreadSanitizer supplies its own operator new and delete, which Clang links
# statically, so targets that replace them to count allocations cannot link.
# Those targets measure allocations, which ThreadSanitizer does not check.
if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*thread")
    set(CPPFIG_THREAD_SANITIZER ON)
else()
    set(CPPFIG_THREAD_SANITIZER OFF)
endif()

if(CPPFIG_BUILD_FUZZERS AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "CPPFIG_BUILD_FUZZERS needs Clang's libFuzzer. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

option(STRICT_BUILD "Enable warnings as errors" ON)

# Warnings live on an INTERFACE target rather than in add_compile_options, so
# they apply to cppfig's own targets and not to dependencies built in-tree or
# to a consumer that adds this project.
add_library(cppfig_warnings INTERFACE)

if(STRICT_BUILD)
    message(STATUS "Strict build enabled — adding comprehensive compiler warnings")

    # Warnings common to GCC and Clang
    target_compile_options(cppfig_warnings INTERFACE
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
        target_compile_options(cppfig_warnings INTERFACE
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
        target_compile_options(cppfig_warnings INTERFACE
            -Wextra-semi
            -Wnewline-eof
            -Wheader-hygiene
            -Wdocumentation
        )
    endif()
endif()

# Uncomment to enable clang-tidy during compilation
# set(CMAKE_CXX_CLANG_TIDY clang-tidy;-fix;-fix-errors)

# GTest for the test suite, and the header self-containment check that src/
# builds alongside the tests.
if(CPPFIG_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(HeaderSelfContainment)
endif()

include(Coverage)

add_subdirectory(src)

if(PROJECT_IS_TOP_LEVEL)
    include(CompileCommands)
    include(Docs)

    # Enable copying of compile_commands.json to project root
    copy_compile_commands()
endif()

if(CPPFIG_BUILD_TESTS OR CPPFIG_BUILD_FUZZERS)
    include(CTest)
endif()

if(CPPFIG_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(CPPFIG_BUILD_TESTS OR CPPFIG_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if(CPPFIG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CPPFIG_BUILD_BENCHMARKS AND NOT CPPFIG_THREAD_SANITIZER)
    add_subdirectory(benchmark)
endif()

# Add coverage targets if coverage is enabled
if(PROJECT_IS_TOP_LEVEL)
    add_coverage_target()
endif()

if(CPPFIG_INSTALL)
    include(Install)
    cppfig_add_install_rules()
endif()
