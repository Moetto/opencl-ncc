#include <iostream>
#include <math.h>
#include "lodepng.h"
#include <sys/time.h>
#include "../lib/timing.h"

using std::vector;
using std::cout;
using std::endl;

struct Image {
    unsigned int height, width;
    vector<unsigned char> pixels;

    uint8_t getPixel(int x, int y) const {
        if (x >= width || y >= height) {
            return 0;
        }
        return pixels[y*width + x];
    }
};

struct Offset {
    int x, y;
};

struct Window {
    vector<Offset> offsets;
    int minX=0, minY=0, maxX=0, maxY=0;

    int minXOffset() {
        if (minX != 0) return minX;
        int min = INT8_MAX;
        for (int i = 0; i < offsets.size(); i++) {
            if (offsets[i].x < min) {
                min = offsets[i].x;
            }
        }
        minX = min;
        return min;
    }

    int minYOffset() {
        if (minY != 0) return minY;
        int min = INT8_MAX;
        for (int i = 0; i < offsets.size(); i++) {
            if (offsets[i].y < min) {
                min = offsets[i].y;
            }
        }
        minY = min;
        return min;
    }

    int maxXOffset() {
        if (maxX != 0) return maxX;
        int max = INT8_MIN;
        for (int i = 0; i < offsets.size(); i++) {
            if (offsets[i].x > max) {
                max = offsets[i].x;
            }
        }
        maxX = max;
        return max;
    }

    int maxYOffset() {
        if (maxY != 0) return maxY;
        int max = INT8_MIN;
        for (int i = 0; i < offsets.size(); i++) {
            if (offsets[i].y > max) {
                max = offsets[i].y;
            }
        }
        maxY = max;
        return max;
    }

    int width() const {
        int minx = INT8_MAX;
        int maxx = INT8_MIN;
        for (int i = 0; i < offsets.size(); i++) {
            int x = offsets[i].x;
            if (x < minx) {
                minx = x;
            }
            if (x > maxx) {
                maxx = x;
            }
        }
        return (unsigned) maxx - minx;
    }

    int height() const {
        int miny = INT8_MAX;
        int maxy = INT8_MIN;
        for (int i = 0; i < offsets.size(); i++) {
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

void encode_to_disk(const char *filename, const std::vector<unsigned char> &image, unsigned width, unsigned height);

vector<uint8_t> get_window_pixels(const Image &image, const int x, const int y, const Window &window_offsets,
                                  const int disparity);

float calculate_mean_value(const vector<uint8_t> &pixels);

float calculate_deviation(const vector<uint8_t> &pixels, const float mean);

Image load_image(const char *filename);

Window construct_border_window(int size);

void resize(std::vector<unsigned char> &out, unsigned &outWidth, unsigned &outHeight,
            const std::vector<unsigned char> &image, const unsigned width, const unsigned height,
            const unsigned factor) {
    const unsigned long length = image.size();

    const int BYTES = 4;

    outWidth = width / factor;
    outHeight = height / factor;

    out.reserve(outWidth * outHeight * BYTES);

    for (unsigned long row = 0; row < height; ++row) {
        if (row % (factor) == 0) {
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
        gs_pixel = 0.2126 * rgb_image[i] + 0.7152 * rgb_image[i + 1] + 0.0722 * rgb_image[i + 2];
        gs_image.push_back(gs_pixel);
    }
}

void encode_gs_to_rgb(const vector<uint8_t> &gs_image, vector<uint8_t> &rgb_image) {
    rgb_image.clear();
    for (uint8_t pixel : gs_image) {
        for (int i = 0; i < 3; i++) {
            rgb_image.push_back(pixel);
        }
        rgb_image.push_back(255);
    }
}

void encode_to_disk(const char *filename, const std::vector<unsigned char> &image,
                    const unsigned width, const unsigned height) {
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
    for (int height = -(win_height / 2); height <= (win_height / 2); height++) {
        for (int width = -(win_width / 2); width <= (win_width / 2); width++) {
            Offset offset = {};
            offset.y = height;
            offset.x = width;
            window.offsets.push_back(offset);
        }
    }
    return window;
}

Window construct_border_window(const int size) {
    Window window;

    if (size == 1) {
        Offset o = {};
        o.x = 0;
        o.y = 0;
        window.offsets.push_back(o);
        return window;
    }

    int win_width = size;
    int win_height = size;

    if (win_width % 2 == 0)
        win_width++;
    if (win_height % 2 == 0)
        win_height++;

    int half_height = (win_height - 1) / 2;
    for (int width = -((win_width - 1) / 2); width <= ((win_width - 1) / 2); width++) {
        Offset offset = {};
        offset.y = half_height;
        offset.x = width;
        window.offsets.push_back(offset);
        offset = {};
        offset.y = -half_height;
        offset.x = width;
        window.offsets.push_back(offset);
    }
    int half_width = (win_width - 1) / 2;
    for (int height = -((win_height - 1) / 2) + 1; height <= ((win_height - 1) / 2) - 1; height++) {
        Offset offset = {};
        offset.y = height;
        offset.x = half_width;
        window.offsets.push_back(offset);
        offset = {};
        offset.y = height;
        offset.x = half_width;
        window.offsets.push_back(offset);
    }
    return window;
}

double calculate_zncc(const vector<uint8_t> &L_pixels, const vector<uint8_t> &R_pixels,
                      const float L_mean, const float R_mean) {
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

Image algorithm(const Image &L_image, const Image &R_image, const int &min_disp,
                const int &max_disp, Window &window) {
    Image output;
    output.width = L_image.width;
    output.height = L_image.height;
    output.pixels = vector<unsigned char>();//output.height * output.width);

    for (int y = 0; y < L_image.height; y++) {
        for (int x = 0; x < L_image.width; x++) {

            // Fill edges with zero
            if (x < -window.minXOffset() || x >= L_image.width - window.maxXOffset() ||
                    y < -window.minYOffset() || y >= L_image.height - window.maxYOffset()) {
                output.pixels.push_back(0);
                continue;
            }

            vector<uint8_t> L_window_pixels = get_window_pixels(L_image, x, y, window, 0);
            float L_mean = calculate_mean_value(L_window_pixels);
            double max_zncc = 0;
            uint8_t best_disp = 0;

            for (int disp = min_disp; disp < max_disp; disp++) {
                // Overflow control
                if (x - disp + window.minXOffset() < 0
                    || x - disp + window.maxXOffset() >= R_image.width) {
                    continue;
                }

                vector<uint8_t> R_window_pixels = get_window_pixels(R_image, x, y, window, disp);
                float R_mean = calculate_mean_value(R_window_pixels);

                // Calculate ZNCC for window
                double zncc = calculate_zncc(L_window_pixels, R_window_pixels, L_mean, R_mean);
                // Update current maximum sum
                if (zncc > max_zncc) {
                    max_zncc = zncc;
                    best_disp = abs(disp);
                }
            }
            output.pixels.push_back(best_disp);
            // output_pixel = best_disp
        }
    }
    return output;
}

vector<uint8_t> get_window_pixels(const Image &image, const int x, const int y,
                                  const Window &window, const int disparity) {
    vector<uint8_t> pixels = vector<uint8_t>();
    for (int i = 0; i < window.offsets.size(); i++) {
        Offset offset = window.offsets[i];
        pixels.push_back(image.pixels[
                                 (y + offset.y) * image.width
                                 + x + offset.x
                                 - disparity
                         ]);
    }
    return pixels;
}

vector<uint8_t> get_available_window_pixels(const Image &image, const unsigned x, const unsigned y,
                                            const Window &window) {
    vector<uint8_t> pixels = vector<uint8_t>();
    int h = image.height;
    int w = image.width;
    for (int i = 0; i < window.offsets.size(); i++) {
        Offset offset = window.offsets[i];
        int real_y = y + offset.y;
        int real_x = x + offset.x;
        if (real_y >= 0 && real_y < h && real_x >= 0 && real_x < w) {
            uint8_t p = image.pixels[
                    (y + offset.y) * image.width
                    + x + offset.x
            ];
            pixels.push_back(p);
        }
    }
    return pixels;
}

float calculate_mean_value(const vector<unsigned char> &pixels) {
    unsigned int sum = 0;
    for (int i = 0; i < pixels.size(); i++) {
        sum += pixels[i];
    }
    return sum / pixels.size();
}

float calculate_deviation(vector<uint8_t> &pixels, float mean, unsigned x_disparity) {
    float sum = 0;

    for (int i = 0; i < pixels.size(); i++) {
        sum += pow(pixels[i - x_disparity] - mean, 2);
    }
    return sqrt(sum / pixels.size());
}

Image load_image(const char *filename, unsigned int factor) {
    unsigned original_width, original_height, smaller_width, smaller_height;
    vector<unsigned char> image = vector<unsigned char>();
    decode(filename, original_width, original_height, image);
    vector<unsigned char> small_image = vector<unsigned char>();
    resize(small_image, smaller_width, smaller_height, image, original_width, original_height, factor);
    vector<uint8_t> gs_image = vector<unsigned char>();
    rgb_to_grayscale(small_image, gs_image);
    Image gs;
    gs.height = smaller_height;
    gs.width = smaller_width;
    gs.pixels = gs_image;
    return gs;
}

Image crossCheck(const Image &i1, const Image &i2, const int threshold, const uint8_t ndisp) {
    Image crossChecked = Image();
    crossChecked.width = i1.width;
    crossChecked.height = i1.height;

    for (int i = 0; i < i1.pixels.size(); i++) {
        uint8_t p1 = i1.pixels[i];
        uint8_t p2 = i2.pixels[i];
        if (abs(p1 - p2) > threshold) {
            crossChecked.pixels.push_back(0);
        } else {
            // Map from 0..ndisp to 0..255
            crossChecked.pixels.push_back(p1 * 255 / ndisp);
        }
    }
    return crossChecked;
}

/**
 * Absolute value of a two-valued vector (euclidean distance from (0,0)
 */
double abs_dist(int x, int y) {
    return sqrt(x*x + y*y);
}

uint8_t findNearestNonZeroPixel(const Image &image, const unsigned int x, const unsigned int y) {
    double closest_dist = -1;
    uint8_t closest_pixel = 0;
    for (int offset = 0; ; offset++) {
        double min_dist = offset;
        if (closest_dist >= 0 && closest_dist <= min_dist) {
            // We can no longer find a closer pixel within this offset
            return closest_pixel;
        }
        for (int xsign = -offset; xsign <= offset; xsign++) {
            for (int ysign = -offset; ysign <= offset; ysign++) {
                if (abs(xsign) < offset && abs(ysign) < offset) {
                    // Don't consider pixels already calculated
                    continue;
                }
                uint8_t pixel = image.getPixel(x + xsign, y + ysign);
                if (pixel != 0) {
                    double dist = abs_dist(xsign, ysign);
                    if (closest_dist < 0 || closest_dist > dist) {
                        closest_dist = dist;
                        closest_pixel = pixel;
                    }
                }
            }
        }
    }
}

Image occlusionFill(const Image &image) {
    Image filled = {};
    filled.width = image.width;
    filled.height = image.height;

    for (unsigned int y = 0; y < image.height; y++) {
        for (unsigned int x = 0; x < image.width; x++) {
            uint8_t closest_pixel;
            if (image.getPixel(x, y)) {
                closest_pixel = image.getPixel(x, y);
            } else {
                closest_pixel = findNearestNonZeroPixel(image, x, y);
            }
            filled.pixels.push_back(closest_pixel);
        }
    }
    return filled;
}

int main(int argc, char *argv[]) {

    Timer timer = Timer();
    timer.start();
    const char *left_name = argc > 1 ? argv[1] : "im0.png";
    const char *right_name = argc > 2 ? argv[2] : "im1.png";
    const char *phase = argc > 3 ? argv[3] : "0";
    const bool save = argc > 4;

    timeval startTime, endTime, startPostProcessing, endCrossCheck;

    gettimeofday(&startTime, NULL);

    Image image1, image2;

    // Maximum disparity value
    const uint8_t ndisp = 64;

    // Cross-check disparity threshold
    const int cc_thresh = 8;

    if (strcmp(phase, "0") == 0) {
        timer.checkPoint("Load images");
        Image left = load_image(left_name, 4);
        Image right = load_image(right_name, 4);
        timer.checkPoint("Begin algorithm");

        //Here goes the algorithm
        Window window = construct_window(9, 9, left.width);
        image1 = algorithm(left, right, 0, ndisp, window);
        cout << "First image ready" << endl;
        image2 = algorithm(right, left, -ndisp, 0, window);
        phase = "1";
        if (save) {
            vector<uint8_t> image1_out, image2_out;
            encode_gs_to_rgb(image1.pixels, image1_out);
            encode_to_disk("zncc1.png", image1_out, image1.width, image1.height);
            encode_gs_to_rgb(image2.pixels, image2_out);
            encode_to_disk("zncc2.png", image2_out, image2.width, image2.height);
        }
    } else {
        image1 = load_image("zncc1.png", 1);
        image2 = load_image("zncc2.png", 1);
    }

    timer.checkPoint("Begin post processing");
    if (strcmp(phase, "1") == 0) {
        Image combined = crossCheck(image1, image2, cc_thresh, ndisp);

        if (save) {
            vector<uint8_t> image_out;
            encode_gs_to_rgb(combined.pixels, image_out);
            encode_to_disk("crosschecked.png", image_out, combined.width, combined.height);
        }

        timer.checkPoint("Begin occlusion fill");
        Image filled = occlusionFill(combined);
        timer.checkPoint("Occlusion fill ready");

        vector<unsigned char> output_image = vector<unsigned char>();
        encode_gs_to_rgb(filled.pixels, output_image);
        encode_to_disk("test.png", output_image, filled.width, filled.height);
    }
    timer.stop();
    return 0;
}
