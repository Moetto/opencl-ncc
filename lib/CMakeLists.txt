cmake_minimum_required(VERSION 3.3)
project(lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        timing.cpp
        timing.h
        opencl-helpers.h
        opencl-helpers.cpp
        lodepng.h
        lodepng.cpp
        )

add_executable(lib ${SOURCE_FILES})