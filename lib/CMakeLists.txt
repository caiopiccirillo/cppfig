add_library(GreeterLib)

target_sources(GreeterLib
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Greeter.cpp
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Greeter.h
    )

target_include_directories(GreeterLib
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    )