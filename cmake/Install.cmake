# Install.cmake
# Install, export and packaging rules for the header-only cppfig target.
#
# Without these, cppfig can only be consumed with add_subdirectory: there is
# nothing to install and nothing for find_package(cppfig) to find, even though
# CPack was being included.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(cppfig_add_install_rules)
    install(TARGETS cppfig
        EXPORT cppfigTargets
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT cppfigTargets
        FILE cppfigTargets.cmake
        NAMESPACE cppfig::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppfig
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cppfig-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cppfig-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppfig
    )

    # Header-only and API-versioned, so any newer cppfig satisfies a request
    # for an older one.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cppfig-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cppfig-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cppfig-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppfig
    )

    set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    include(CPack)
endfunction()
