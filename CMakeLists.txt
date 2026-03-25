cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
project(iotServer VERSION 1.0)

find_package(mysocket REQUIRED)

add_executable(server main.cpp)
target_link_libraries(server mylibs::mysocket)
