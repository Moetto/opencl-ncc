#include <iostream>
#include <stdio.h>
#include "lodepng.h"

using namespace std;

void decode(const char* filename) {
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;

    //decode
    unsigned error = lodepng::decode(image, width, height, filename);

    //if there's an error, display it
    if(error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
}

int main(int argc, char *argv[]) {

    const char* filename = argc > 1 ? argv[1] : "im0.png";
    decode(filename);

    return 0;
}
