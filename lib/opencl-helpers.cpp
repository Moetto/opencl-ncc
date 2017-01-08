//
// Created by moetto on 1/8/17.
//
#include "opencl-helpers.h"
#include "lodepng.h"

#include <string>
#include <vector>

using std::string;
using std::vector;


void save_image_to_disk(const string &filename, cl::CommandQueue &queue, cl::Image2D &image,
                        const cl::size_t<3> &start, const cl::size_t<3> &end) {
    unsigned w = end[0] - start[0], h = end[1] - start[1];
    vector<uint8_t > output = vector<uint8_t >(w*h*4);
    queue.enqueueReadImage(image, CL_TRUE, start, end, 0, 0, &output[0], NULL, NULL);
    lodepng::encode(filename.c_str(), output, w, h);
}

void decode(const char *filename, unsigned &width, unsigned &height, std::vector<unsigned char> &image) {
    lodepng::decode(image, width, height, filename);
}

Image load_image(const char *filename) {
    unsigned original_width, original_height;
    vector<unsigned char> image = vector<unsigned char>();
    decode(filename, original_width, original_height, image);
    Image img;
    img.height = original_height;
    img.width = original_width;
    img.pixels = image;
    return img;
}

