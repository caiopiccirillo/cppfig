# This module provides a function to copy compile_commands.json from the build
# directory to the project root. This is useful for projects with multiple
# build configurations where you need to keep clangd updated with the current
# setup.

function(copy_compile_commands)
    # Only proceed if CMAKE_EXPORT_COMPILE_COMMANDS is enabled
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        message(STATUS "CompileCommands: CMAKE_EXPORT_COMPILE_COMMANDS is not enabled, skipping copy")
        return()
    endif()

    set(SOURCE_FILE "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(DEST_FILE "${CMAKE_SOURCE_DIR}/compile_commands.json")

    # Create a custom target that will copy the file
    add_custom_target(copy_compile_commands_target ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOURCE_FILE}"
            "${DEST_FILE}"
        COMMENT "Copying compile_commands.json to project root"
        VERBATIM
    )

    # Set the custom target to run late in the build process
    set_target_properties(copy_compile_commands_target PROPERTIES
        FOLDER "CMakePredefinedTargets"
    )

    # Also create a configure-time copy if the source file already exists
    if(EXISTS "${SOURCE_FILE}")
        message(STATUS "CompileCommands: Found existing compile_commands.json, copying to project root")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOURCE_FILE}"
                "${DEST_FILE}"
            RESULT_VARIABLE copy_result
        )
        if(copy_result EQUAL 0)
            message(STATUS "CompileCommands: Successfully copied compile_commands.json")
        else()
            message(WARNING "CompileCommands: Failed to copy compile_commands.json")
        endif()
    endif()

    message(STATUS "CompileCommands: Configured to copy compile_commands.json from ${SOURCE_FILE} to ${DEST_FILE}")
endfunction()

# Convenience macro to automatically call the function
macro(enable_compile_commands_copy)
    copy_compile_commands()
endmacro()

# Helper function to force regeneration of compile_commands.json and copy it
function(update_compile_commands)
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        message(FATAL_ERROR "CMAKE_EXPORT_COMPILE_COMMANDS must be enabled to update compile commands")
    endif()

    set(SOURCE_FILE "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(DEST_FILE "${CMAKE_SOURCE_DIR}/compile_commands.json")

    # Force copy the file
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy
            "${SOURCE_FILE}"
            "${DEST_FILE}"
        RESULT_VARIABLE copy_result
    )

    if(copy_result EQUAL 0)
        message(STATUS "CompileCommands: Force-updated compile_commands.json in project root")
    else()
        message(FATAL_ERROR "CompileCommands: Failed to copy compile_commands.json")
    endif()
endfunction()
