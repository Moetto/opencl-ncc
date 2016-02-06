#include <iostream>
#include <stdio.h>
#include "lodepng.h"

using namespace std;

void encode_to_disk(const char *filename, std::vector<unsigned char> &image, unsigned width, unsigned height);

float calculate_mean_value(const vector<uint8_t> &, const vector<std::pair<int, int>> &, unsigned);

void resize(std::vector<unsigned char> &out, unsigned &outWidth, unsigned &outHeight,
            const std::vector<unsigned char> &image, const unsigned width, const unsigned height,
            const unsigned factor) {
    const unsigned long length = image.size();

    const int BYTES = 4;

    outWidth = width / factor;
    outHeight = height / factor;

    out.reserve(outWidth * outHeight * BYTES);

    for (unsigned long row = 0; row < height; ++row) {
        if (row % BYTES == 0) {
            for (unsigned long column = 0; column < width * BYTES; column += BYTES) {
                if (column % (factor * BYTES) == 0) {
                    const unsigned long i = row * width * BYTES + column;
                    out.push_back(image.at(i));
                    out.push_back(image.at(i + 1));
                    out.push_back(image.at(i + 2));
                    out.push_back(image.at(i + 3));
                }
            }
        }
    }

}

void decode(const char *filename, unsigned &width, unsigned &height, vector<unsigned char> &image) {
    //decode
    unsigned error = lodepng::decode(image, width, height, filename);

    //if there's an error, display it
    if (error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
}

void rgb_to_grayscale(const vector<unsigned char> &rgb_image, vector<unsigned char> &gs_image) {

    uint8_t gs_pixel;
    for (int i = 0; i < rgb_image.size(); i += 4) {
        gs_pixel = 0.2126 * rgb_image[i] + +0.7152 * rgb_image[i + 1] + 0.0722 * rgb_image[i + 2];
        gs_image.push_back(gs_pixel);
    }
}

void encode_gs_to_rgb(const vector<uint8_t> &gs_image, vector<uint8_t> &rgb_image) {
    rgb_image.clear();
    for (uint8_t pixel : gs_image) {
        for (int i = 0; i < 3; i++) {
            rgb_image.push_back((unsigned char) pixel);
        }
        rgb_image.push_back(255);
    }
}

void encode_to_disk(const char *filename, std::vector<unsigned char> &image, unsigned width, unsigned height) {
    //Encode the image
    unsigned error = lodepng::encode(filename, image, width, height);

    //if there's an error, display it
    if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}


/* Constructs a vector of offsets that describes the window of pixels relative to a point in an image
 * Currently constructs only a basic square
 */
vector<int> construct_window(const unsigned win_width, const int win_height, const int im_width) {
    unsigned win_size = win_width * win_height;
    vector<int> window = vector<int>(win_size);
    for (int j = 0; j < win_height; j++) {
        for (int i = 0; i < win_width; i++) {
            window.push_back(j * im_width + i);
        }
    }

    return window;
}

void algorithm(const vector<uint8_t> &gs_image, unsigned width, unsigned height, unsigned max_disp,
               vector<pair<int, int>> &window) {

    for (unsigned w = 1; w < width - 1; w++) {
        for (int h = 1; h < height - 1; h++) {
            for (int disp = 0; disp < max_disp; disp++) {
                calculate_mean_value(gs_image, window, width);
            }
        }
    }
}

float calculate_mean_value(const vector<uint8_t> &image, const vector<std::pair<int, int>> &window,
                           unsigned width) {
    uint8_t sum = 0;
    for (int i = 0; i < window.size(); i++) {
        pair<int, int> offset = window[i];
        sum += image[offset.first + offset.second * width];
    }
    return sum / window.size();
}

int main(int argc, char *argv[]) {

    const char *filename = argc > 1 ? argv[1] : "im0.png";
    unsigned original_width, original_height, smaller_width, smaller_height;
    vector<unsigned char> image = vector<unsigned char>();
    decode(filename, original_width, original_height, image);
    vector<unsigned char> small_image = vector<unsigned char>();
    resize(small_image, smaller_width, smaller_height, image, original_width, original_height, 4);
    vector<uint8_t> gs_image = vector<unsigned char>();
    rgb_to_grayscale(small_image, gs_image);
    vector<unsigned char> output_image = vector<unsigned char>();
    encode_gs_to_rgb(gs_image, output_image);
    encode_to_disk("test.png", output_image, smaller_width, smaller_height);

    return 0;
}
