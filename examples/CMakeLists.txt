cmake_minimum_required(VERSION 3.22.0)

add_executable(cppfig_example simple_config_example.cpp)
target_link_libraries(cppfig_example PRIVATE cppfig)
