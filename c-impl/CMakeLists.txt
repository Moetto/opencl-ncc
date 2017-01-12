cmake_minimum_required(VERSION 3.3)
project(opencl_ncc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        main.cpp
        lodepng.h
        lodepng.cpp
        ../lib/timing.h
        ../lib/timing.cpp)

add_executable(opencl_ncc ${SOURCE_FILES})