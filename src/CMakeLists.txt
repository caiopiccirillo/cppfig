add_library(cppfig)

target_sources(cppfig
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.cpp
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.h
    ${CMAKE_CURRENT_LIST_DIR}/IConfiguration.h
)

target_include_directories(cppfig
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(cppfig
    PUBLIC
    nlohmann_json::nlohmann_json
)

if(STRICT_BUILD)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(cppfig PRIVATE -Wall -Wextra -Wpedantic -Werror -Wdocumentation)
    else()
        target_compile_options(cppfig PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
endif(STRICT_BUILD)
