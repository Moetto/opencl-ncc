#include <iostream>
#include <stdio.h>
#include <complex>
#include "lodepng.h"

using namespace std;

struct Image {
    unsigned int height, width;
    vector<unsigned char> pixels;
};

struct Offset {
    int x, y;
};

struct Window {
    vector<Offset> offsets;

    unsigned int maxXOffset() {
        int max = INT8_MIN;
        for (int i=0; i< offsets.size(); i++) {
            if (offsets[i].x > max) {
                max = offsets[i].x;
            }
        }
        return max;
    }
    unsigned int maxYOffset() {
        int max = INT8_MIN;
        for (int i=0; i< offsets.size(); i++) {
            if (offsets[i].y > max) {
                max = offsets[i].y;
            }
        }
        return max;
    }

    unsigned int width() {
        int minx = INT8_MIN;
        int maxx = INT8_MAX;
        for (int i=0; i<offsets.size(); i++) {
            int x = offsets[i].x;
            if (x < minx) {
                minx = x;
            }
            if (x > maxx) {
                maxx = x;
            }
        }
        return maxx - minx;
    }

    unsigned int height() {
        int miny = INT8_MIN;
        int maxy = INT8_MAX;
        for (int i=0; i<offsets.size(); i++) {
            int y = offsets[i].y;
            if (y < miny) {
                miny = y;
            }
            if (y > maxy) {
                maxy = y;
            }
        }
        return maxy - miny;
    }
};

void encode_to_disk(const char *filename, std::vector<unsigned char> &image, unsigned width, unsigned height);

vector<uint8_t> get_window_pixels(const Image &image, unsigned x, unsigned y, Window window_offsets,
                                  int disparity);

float calculate_mean_value(vector<uint8_t> pixels);

float calculate_deviation(vector<uint8_t> pixels, float mean);

Image load_image(const char *filename);

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
Window construct_window(const int win_width, const int win_height, const int im_width) {

    unsigned win_size = win_width * win_height;
    Window window;  // = vector<Offset>(win_size);
    for (int height = -1; height < win_height - 1; height++) {
        for (int width = -1; width < win_width - 1; width++) {
            Offset offset = {};
            offset.y = height;
            offset.x = width;
            window.offsets.push_back(offset);
        }
    }
    return window;
}

double calculate_zncc(vector<uint8_t> L_pixels, vector<uint8_t> R_pixels, float L_mean, float R_mean) {
    double upper_sum = 0;
    double lower_l_sum = 0;
    double lower_r_sum = 0;
    for (int i = 0; i < L_pixels.size(); i++) {
        double L = (L_pixels[i] - L_mean);
        double R = (R_pixels[i] - R_mean);
        upper_sum += L * R;
        lower_l_sum += pow(L, 2);
        lower_r_sum += pow(R, 2);
    }

    return upper_sum / (sqrt(lower_l_sum) * sqrt(lower_r_sum));
}

Image algorithm(Image L_image, Image R_image, unsigned max_disp, Window &window) {
    Image output;
    output.width = L_image.width - window.width();
    output.height = L_image.height - window.height();
    output.pixels = vector<unsigned char>();//output.height * output.width);

    for (unsigned y = window.maxYOffset(); y < L_image.height - window.maxYOffset(); y++) {
        for (unsigned x = window.maxXOffset(); x < L_image.width - window.maxXOffset(); x++) {
            vector<uint8_t> L_window_pixels = get_window_pixels(L_image, x, y, window, 0);
            float L_mean = calculate_mean_value(L_window_pixels);
            double max_zncc = 0;
            int best_disp = 0;
            for (int disp = 0; disp < max_disp; disp++) {
                vector<uint8_t> R_window_pixels = get_window_pixels(R_image, x, y, window, disp);
                float R_mean = calculate_mean_value(R_window_pixels);

                // Calculate ZNCC for window
                double zncc = calculate_zncc(L_window_pixels, R_window_pixels, L_mean, R_mean);
                // Update current maximum sum
                if (zncc > max_zncc) {
                    max_zncc = zncc;
                    best_disp = disp;
                }
            }
            output.pixels.push_back(best_disp*255/max_disp);
            // output_pixel = best_disp
        }
    }
    return output;
}

vector<uint8_t> get_window_pixels(const Image &image, unsigned x, unsigned y, Window window, int disparity) {
    vector<uint8_t> pixels = vector<uint8_t>();
    for (int i = 0; i < window.offsets.size(); i++) {
        Offset offset = window.offsets[i];
        pixels.push_back(image.pixels[y*image.height+offset.y + x + offset.x - disparity]);
    }
    return pixels;
}

float calculate_mean_value(vector<unsigned char> pixels) {
    unsigned int sum = 0;
    for (int i = 0; i < pixels.size(); i++) {
        sum += pixels[i];
    }
    return sum / pixels.size();
}

float calculate_deviation(vector<uint8_t> pixels, float mean, unsigned x_disparity) {
    float sum = 0;

    for (int i = 0; i < pixels.size(); i++) {
        sum += pow(pixels[i - x_disparity] - mean, 2);
    }
    return sqrt(sum / pixels.size());
}

Image load_image(const char *filename) {
    unsigned original_width, original_height, smaller_width, smaller_height;
    vector<unsigned char> image = vector<unsigned char>();
    decode(filename, original_width, original_height, image);
    vector<unsigned char> small_image = vector<unsigned char>();
    resize(small_image, smaller_width, smaller_height, image, original_width, original_height, 4);
    vector<uint8_t> gs_image = vector<unsigned char>();
    rgb_to_grayscale(small_image, gs_image);
    Image gs;
    gs.height = smaller_height;
    gs.width = smaller_width;
    gs.pixels = gs_image;
    return gs;
}

int main(int argc, char *argv[]) {

    const char *left_name = argc > 1 ? argv[1] : "im0.png";
    const char *right_name = argc > 2 ? argv[2] : "im1.png";

    Image left = load_image(left_name);
    Image right = load_image(right_name);

    //Here goes the algorithm
    vector<Offset> window = construct_window(9, 9, left.width);
    Image output = algorithm(left, right, 90, window);
    cout << output.width << endl;
    vector<unsigned char> output_image = vector<unsigned char>();
    encode_gs_to_rgb(output.pixels, output_image);
    encode_to_disk("test.png", output_image, output.width, output.height);

    return 0;
}
