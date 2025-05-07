cmake_minimum_required(VERSION 3.22.0)

add_executable(cppfig-example example.cpp)
target_link_libraries(cppfig-example PRIVATE cppfig)
