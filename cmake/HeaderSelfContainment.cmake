# HeaderSelfContainment.cmake
# CMake module for verifying that every public header is self-contained.
#
# A header-only library must be includable in any order, so each header has to
# pull in everything it uses. Transitive includes from the standard library
# hide missing ones until a toolchain upgrade rearranges them — which is how
# status.h shipped without <cstdint>.
#
# Generates one translation unit per header, each including only that header,
# and compiles them as an object library under the project's normal warning
# settings.
#
# Usage:
#   add_header_self_containment_target(cppfig_header_check
#       LINK_LIBRARIES cppfig::cppfig
#       HEADERS cppfig/status.h cppfig/value.h)

function(add_header_self_containment_target target)
    cmake_parse_arguments(ARG "" "" "HEADERS;LINK_LIBRARIES" ${ARGN})

    if(NOT ARG_HEADERS)
        message(FATAL_ERROR "add_header_self_containment_target: no HEADERS given")
    endif()

    set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/header_self_containment")
    set(_sources "")

    # HEADERS are include paths as a consumer would write them, so each one can
    # be dropped straight into a #include.
    foreach(_include_path ${ARG_HEADERS})
        # Derive a unique TU name from the header's path: cppfig/testing/mock.h
        # becomes cppfig_testing_mock_h.cpp.
        string(REGEX REPLACE "[/.]" "_" _stem "${_include_path}")
        set(_source "${_generated_dir}/${_stem}.cpp")

        file(GENERATE OUTPUT "${_source}" CONTENT "#include <${_include_path}>\n")
        list(APPEND _sources "${_source}")
    endforeach()

    add_library(${target} OBJECT ${_sources})
    target_link_libraries(${target} PRIVATE ${ARG_LINK_LIBRARIES})
endfunction()
