#include <iostream>
#include <stdio.h>
#include "lodepng.h"

using namespace std;

void resize(std::vector<unsigned char> & out, unsigned & outWidth, unsigned & outHeight,
            const std::vector<unsigned char> * image, const unsigned width, const unsigned height,
            const unsigned factor) {
    const unsigned long length = image->size();

    const int BYTES = 4;

    outWidth = width / factor;
    outHeight = height / factor;

    out.reserve(outWidth*outHeight*BYTES);

    for (unsigned long row=0; row < height; ++row) {
        for (unsigned long column = 0; column < width * BYTES; column+= BYTES) {
            if (row % BYTES == 0) {
                if (column % (factor * BYTES) == 0) {
                    const unsigned long i = row*width + column;
                    out.push_back(image->at(i));
                    out.push_back(image->at(i+1));
                    out.push_back(image->at(i+2));
                    out.push_back(image->at(i+3));
                }
            }
        }
    }

}

void decode(const char* filename) {
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;
    unsigned resizedWidth, resizedHeight;

    //decode
    unsigned error = lodepng::decode(image, width, height, filename);

    //if there's an error, display it
    if(error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    std::vector<unsigned char> resized;
    resize(resized, resizedWidth, resizedHeight, &image, width, height, 4);

    int a = 3;
    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
}

int main(int argc, char *argv[]) {
    cout << "Hello world!\n";
    printf("Hello world in C!\n");

    const char* filename = argc > 1 ? argv[1] : "test.png";
    decode(filename);

    return 0;
}
