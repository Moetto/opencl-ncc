#include <vector>
#include "lodepng.h"
#include <stdlib.h>

#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>
#include <iostream>
#include <fstream>
#include <malloc.h>

using std::vector;
using std::cout;
using std::endl;



struct Image {
    ::size_t height, width;
    vector<unsigned char> pixels;
};


void decode(const char *filename, unsigned &width, unsigned &height, vector<unsigned char> &image) {
    lodepng::decode(image, width, height, filename);
}

void encode_to_disk(const char *filename, std::vector<unsigned char> &image, unsigned width, unsigned height) {
    //Encode the image
    unsigned error = lodepng::encode(filename, image, width, height);

    //if there's an error, display it
    if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
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

int main(int argc, char *argv[]) {
    const char *left_name = argc > 1 ? argv[1] : "im0.png";
    const char *right_name = argc > 2 ? argv[2] : "im1.png";

    Image left = load_image(left_name);
    Image right = load_image(right_name);

    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    vector<cl::Device> devices;
    platforms[0].getDevices(CL_DEVICE_TYPE_GPU, &devices);

    cout << "Creating context" << endl;
    cl::Context ctx = cl::Context(
            devices,
            NULL,
            NULL,
            NULL,
            NULL
    );

    std::string vendor, deviceName;
    platforms[0].getInfo(CL_PLATFORM_VENDOR, &vendor);

    cout << "Selected vendor " << vendor << " and devices " << endl;
    for (auto device: devices) {
        std::string name;
        device.getInfo(CL_DEVICE_NAME, &name);
        cout << name << endl;
    }

    cl::CommandQueue queue = cl::CommandQueue(ctx, devices[0]);

    std::ifstream resize_kernel_file("resize.cl");
    std::string resize_text((std::istreambuf_iterator<char>(resize_kernel_file)),
                            std::istreambuf_iterator<char>());

    cl::Program program = cl::Program(ctx,
                                      cl::Program::Sources(1, std::make_pair(resize_text.c_str(), resize_text.size())));

    try {
        program.build(devices);
    } catch (const cl::Error &) {
        std::cerr
        << "OpenCL compilation error" << endl
        << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0])
        << std::endl;
        return 1;
    }

    cl::ImageFormat imageFormat;
    imageFormat.image_channel_order = CL_RGBA;
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;

    cl_int image_err;

    cl::Image2D original;
    cl::Image2D resized;

    try {
        original = cl::Image2D(ctx, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, imageFormat, left.width, left.height, 0,
                               &left.pixels[0], &image_err);
        resized = cl::Image2D(ctx, CL_MEM_WRITE_ONLY, imageFormat,
                              left.width / 4, left.height / 4, 0,
                              NULL, &image_err);
    } catch (const cl::Error &ex) {
        std::cerr
        << "Error creating image " << endl << ex.what() << " " << ex.err() << endl << "Error " << image_err << endl;
        return 1;
    }

    if (image_err == CL_SUCCESS)
        cout << "Image created successfully " << endl;
    else
        return 1;

    cl::Kernel resize(program, "resize");

    try {
        resize.setArg(0, left.height);
        resize.setArg(1, left.width);
        resize.setArg(2, original);
        resize.setArg(3, resized);
        resize.setArg(4, left.width * left.height);
    } catch (const cl::Error &ex) {
        std::cerr << ex.what() << " " << ex.err() << endl;
        return 1;
    }

    cout << "Putting kernel to work" << endl;
    cl_int err;
    cl::Event event;

    err = queue.enqueueNDRangeKernel(resize, cl::NullRange, cl::NDRange(left.width, left.height), cl::NullRange,
                                     NULL,
                                     &event);
    if (err != CL_SUCCESS) {
        cout << "Error in queue " << err << endl;
        return 1;
    }
    cout << "Kernel working" << endl;

    vector<uint8_t> output(left.height * left.width / 4);

    cl::size_t<3> start;
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;

    cl::size_t<3> end;
    end[0] = 100;
    end[1] = 100;

    end[0] = left.width/4;
    end[1] = left.height/4;
    end[2] = 1;

    cout << "Waiting" << endl;
    event.wait();
    cout << "Ready" << endl;
    try {
        err = queue.enqueueReadImage(resized, CL_TRUE, start, end, 0, 0, &output[0], NULL, NULL);
    } catch (const cl::Error &ex) {
        std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
        return 1;
    }
    event.wait();

    if (err != CL_SUCCESS) {
        cout << err << endl;
        return 1;
    }

    cout << "Mem copy ready" << endl;

    encode_to_disk("test.png", output, unsigned(left.width)/4, unsigned(left.height)/4);

    cout << "Bye" << endl;
}