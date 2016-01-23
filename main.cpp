#include <iostream>
#include <stdio.h>
#include "lodepng.h"

using namespace std;

void encode_to_disk (const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height);

void decode(const char *filename, unsigned& width, unsigned& height, vector<unsigned char>& image) {
    //decode
    unsigned error = lodepng::decode(image, width, height, filename);

    //if there's an error, display it
    if(error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
}


void encode_to_disk(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height) {
    //Encode the image
    unsigned error = lodepng::encode(filename, image, width, height);

    //if there's an error, display it
    if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

int main(int argc, char *argv[]) {

    const char *filename = argc > 1 ? argv[1] : "im0.png";
    unsigned width, height;
    vector<unsigned char> image = vector<unsigned char>();
    decode(filename, width, height, image);

    return 0;
}
