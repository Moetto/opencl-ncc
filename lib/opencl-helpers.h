//
// Created by moetto on 1/8/17.
//

#ifndef LIB_OPENCL_HELPERS_H
#define LIB_OPENCL_HELPERS_H

#include <string>
#include <vector>

#include "../opencl-impl/cl.hpp"

struct Image {
    ::size_t height, width;
    std::vector<unsigned char> pixels;
};

void save_image_to_disk(const std::string &filename, cl::CommandQueue &queue, cl::Image2D &image, const cl::size_t<3> &start, const cl::size_t<3> &end);
Image load_image(const char *filename);

#endif //LIB_OPENCL_HELPERS_H
