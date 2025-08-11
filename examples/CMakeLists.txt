cmake_minimum_required(VERSION 3.22.0)

add_executable(cppfig_example simple_config_example.cpp)
target_link_libraries(cppfig_example PRIVATE cppfig)

add_executable(custom_types_example custom_types_example.cpp)
target_link_libraries(custom_types_example PRIVATE cppfig)
