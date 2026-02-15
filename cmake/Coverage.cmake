# Coverage.cmake
# CMake module for enabling code coverage with gcov/llvm-cov

option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Add coverage flags
        set(COVERAGE_COMPILE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(COVERAGE_LINK_FLAGS "--coverage")

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILE_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}")

        # For Clang, use llvm-cov; for GCC, use gcov
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(GCOV_TOOL "llvm-cov gcov")
        else()
            set(GCOV_TOOL "gcov")
        endif()
    else()
        message(WARNING "Code coverage is only supported with GCC or Clang")
    endif()
endif()

# Function to add a coverage target
function(add_coverage_target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    find_program(GCOVR_PATH gcovr)

    if(GCOVR_PATH)
        # Use gcovr for coverage report generation
        # Use --filter to only include src/cppfig/ directory (our library code)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${GCOVR_PATH}
                --root ${CMAKE_SOURCE_DIR}
                --filter ".*src/cppfig/.*"
                --html --html-details
                --output ${CMAKE_BINARY_DIR}/coverage/index.html
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated: ${CMAKE_BINARY_DIR}/coverage/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report with gcovr"
            VERBATIM
        )

        add_custom_target(coverage-text
            COMMAND ${GCOVR_PATH}
                --root ${CMAKE_SOURCE_DIR}
                --filter ".*src/cppfig/.*"
                --print-summary
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage summary"
            VERBATIM
        )

        add_custom_target(coverage-xml
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${GCOVR_PATH}
                --root ${CMAKE_SOURCE_DIR}
                --filter ".*src/cppfig/.*"
                --xml
                --output ${CMAKE_BINARY_DIR}/coverage/coverage.xml
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage XML report: ${CMAKE_BINARY_DIR}/coverage/coverage.xml"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage XML report"
            VERBATIM
        )
    elseif(LCOV_PATH AND GENHTML_PATH)
        # Fallback to lcov/genhtml
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --zerocounters
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info
                '${CMAKE_SOURCE_DIR}/test/*'
                '${CMAKE_SOURCE_DIR}/benchmark/*'
                '${CMAKE_SOURCE_DIR}/examples/*'
                '${CMAKE_SOURCE_DIR}/third/*'
                '${CMAKE_BINARY_DIR}/*'
                '/usr/*'
                --output-file coverage.info.cleaned
            COMMAND ${GENHTML_PATH} coverage.info.cleaned --output-directory coverage
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated: ${CMAKE_BINARY_DIR}/coverage/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report with lcov"
            VERBATIM
        )
    else()
        message(WARNING "Neither gcovr nor lcov/genhtml found. Install gcovr for coverage reports: pip install gcovr")
    endif()
endfunction()
