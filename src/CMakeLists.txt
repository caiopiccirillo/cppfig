add_library(CppfigLib)

target_sources(CppfigLib
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.cpp
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.h
    ${CMAKE_CURRENT_LIST_DIR}/IConfiguration.h
)

target_include_directories(CppfigLib
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(CppfigLib
    PUBLIC
    # nlohmann_json::nlohmann_json
    # spdlog::spdlog
)

if(STRICT_BUILD)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(CppfigLib PRIVATE -Wall -Wextra -Wpedantic -Werror -Wdocumentation)
    else()
        target_compile_options(CppfigLib PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
endif(STRICT_BUILD)
