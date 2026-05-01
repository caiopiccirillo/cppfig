# Docs.cmake
# CMake module for building documentation with Doxygen + doxygen-awesome-css.
#
# On first configure (or when the theme is missing), downloads the
# doxygen-awesome-css theme to CMAKE_BINARY_DIR/docs-theme/ so that
# the generated HTML uses a modern, responsive style.
#
# Usage:
#   cmake -B build
#   cmake --build build --target docs
#
# Public targets added:
#   docs            – build the Doxygen HTML documentation
#   docs-serve      – (optional) start a Python http.server for preview

include(FetchContent)

# Only enable documentation targets if Doxygen is available.
find_package(Doxygen COMPONENTS dot QUIET)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found — documentation targets disabled")
    return()
endif()

message(STATUS "Doxygen found: ${DOXYGEN_VERSION}")

set(_DOXYGEN_AWESOME_VERSION "2.3.4")
set(_DOXYGEN_AWESOME_DIR "${CMAKE_BINARY_DIR}/docs-theme")
set(_DOXYGEN_AWESOME_URL
    "https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v${_DOXYGEN_AWESOME_VERSION}.zip"
)

# Download / verify the theme on first configure.
if(NOT EXISTS "${_DOXYGEN_AWESOME_DIR}/doxygen-awesome.css")
    message(STATUS "Downloading doxygen-awesome-css v${_DOXYGEN_AWESOME_VERSION} ...")

    file(DOWNLOAD
        ${_DOXYGEN_AWESOME_URL}
        "${CMAKE_BINARY_DIR}/doxygen-awesome-css.zip"
        SHOW_PROGRESS
        STATUS _download_status
    )

    list(GET _download_status 0 _download_rc)
    if(NOT _download_rc EQUAL 0)
        list(GET _download_status 1 _download_err)
        message(FATAL_ERROR "Failed to download doxygen-awesome-css: ${_download_err}")
    endif()

    file(ARCHIVE_EXTRACT
        INPUT "${CMAKE_BINARY_DIR}/doxygen-awesome-css.zip"
        DESTINATION "${CMAKE_BINARY_DIR}/_doxygen-awesome-extract"
    )

    # The extracted folder is versioned; copy the files we need into the
    # final location so the Doxyfile can reference them predictably.
    file(GLOB _extracted_root
        "${CMAKE_BINARY_DIR}/_doxygen-awesome-extract/doxygen-awesome-css-*"
    )
    list(GET _extracted_root 0 _extracted_dir)

    file(COPY
        "${_extracted_dir}/doxygen-awesome.css"
        "${_extracted_dir}/doxygen-awesome-sidebar-only.css"
        "${_extracted_dir}/doxygen-awesome-darkmode-toggle.js"
        "${_extracted_dir}/doxygen-awesome-fragment-copy-button.js"
        "${_extracted_dir}/doxygen-awesome-paragraph-link.js"
        "${_extracted_dir}/doxygen-awesome-interactive-toc.js"
        DESTINATION "${_DOXYGEN_AWESOME_DIR}"
    )

    # Clean up temporary artefacts.
    file(REMOVE_RECURSE
        "${CMAKE_BINARY_DIR}/_doxygen-awesome-extract"
        "${CMAKE_BINARY_DIR}/doxygen-awesome-css.zip"
    )

    message(STATUS "doxygen-awesome-css installed in ${_DOXYGEN_AWESOME_DIR}")
else()
    message(STATUS "doxygen-awesome-css already present in ${_DOXYGEN_AWESOME_DIR}")
endif()

# Create the 'docs' target.
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Doxyfile"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Doxygen documentation"
    VERBATIM
)

# Convenience target to preview the docs locally.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(docs-serve
        COMMAND ${Python3_EXECUTABLE} -m http.server 8080
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build/docs/html
        COMMENT "Serving documentation at http://localhost:8080"
        VERBATIM
    )
endif()
