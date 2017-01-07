cmake_minimum_required(VERSION 3.3)
project(opencl_impl)

find_package(OpenCL REQUIRED)

include_directories(${OpenCL_INCLUDE_DIRS})
link_directories(${OpenCL_LIBRARY})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        main.cpp
        resize.cl
        lodepng.h
        lodepng.cpp
        ../lib/timing.h
        ../lib/timing.cpp
        )

add_executable(opencl_impl ${SOURCE_FILES})

target_link_libraries (opencl_impl ${OpenCL_LIBRARY})


