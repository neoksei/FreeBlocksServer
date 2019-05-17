cmake_minimum_required(VERSION 3.14)
project(FreeBlocks)

set(CMAKE_CXX_STANDARD 17)

add_executable(FreeBlocks main.cpp FileSystem.cpp FileSystem.h)

find_package(cppzmq)
target_link_libraries(FreeBlocks cppzmq)