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
using std::string;


struct Image {
    ::size_t height, width;
    vector<unsigned char> pixels;
};

struct imageSet {
    Image originalImage;
    cl::Image2D original, gs, meaned, znccd, output;
    string fileName;
} left, right;


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
    left.fileName = "left.png";
    right.fileName = "right.png";

    left.originalImage = load_image(left_name);
    right.originalImage = load_image(right_name);

    Image resizedImage = {};
    resizedImage.width = left.originalImage.width / 4;
    resizedImage.height = left.originalImage.height / 4;

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


    size_t h = left.originalImage.height, w = left.originalImage.width;
    try {
        left.original = cl::Image2D(ctx, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, imageFormat, w,
                                    h, 0, &left.originalImage.pixels[0], &image_err);
        left.gs = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                              NULL, &image_err);
        left.meaned = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                  NULL, &image_err);
        left.znccd = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0, NULL,
                                 &image_err);
        left.output = cl::Image2D(ctx, CL_MEM_WRITE_ONLY, imageFormat, resizedImage.width, resizedImage.height, 0, NULL,
                                  &image_err);


        right.original = cl::Image2D(ctx, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, imageFormat, w,
                                     h, 0, &right.originalImage.pixels[0], &image_err);
        right.gs = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                               NULL, &image_err);
        right.meaned = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                   NULL, &image_err);
        right.znccd = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                  NULL, &image_err);
        right.output = cl::Image2D(ctx, CL_MEM_WRITE_ONLY, imageFormat, resizedImage.width, resizedImage.height, 0,
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
    cl::Kernel mean(program, "calculate_mean");
    cl::Kernel zncc(program, "calculate_zncc");

    try {
        resize.setArg(0, h);
        resize.setArg(1, w);
        resize.setArg(4, h * w);
        mean.setArg(2, 4);
    } catch (const cl::Error &ex) {
        std::cerr << ex.what() << " " << ex.err() << endl;
        return 1;
    }

    for (auto set : {left, right}) {
        try {
            resize.setArg(2, set.original);
            resize.setArg(3, set.gs);
            mean.setArg(0, set.gs);
            mean.setArg(1, set.meaned);
        } catch (const cl::Error &ex) {
            std::cerr << ex.what() << " " << ex.err() << endl;
            return 1;
        }

        cout << "Putting kernel to work" << endl;
        cl_int err;
        cl::Event e1, e2;

        err = queue.enqueueNDRangeKernel(resize, cl::NullRange, cl::NDRange(w, h), cl::NullRange, NULL, &e1);
        vector<cl::Event> await = vector<cl::Event>();
        await.push_back(e1);
        err = queue.enqueueNDRangeKernel(mean, cl::NullRange, cl::NDRange(resizedImage.width, resizedImage.height),
                                         cl::NullRange, &await, &e2);
        if (err != CL_SUCCESS) {
            cout << "Error in queue " << err << endl;
            return 1;
        }
        cout << "Kernel working" << endl;

        vector<uint8_t> output(resizedImage.height * resizedImage.width * 4);

        cl::size_t<3> start;
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;

        cl::size_t<3> end;
        end[0] = resizedImage.width;
        end[1] = resizedImage.height;
        end[2] = 1;

        cout << "Waiting" << endl;
        e2.wait();

        cout << "Ready" << endl;

        try {
            err = queue.enqueueReadImage(set.meaned, CL_TRUE, start, end, 0, 0, &output[0], NULL, NULL);
        } catch (const cl::Error &ex) {
            std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
            return 1;
        }

        if (err != CL_SUCCESS) {
            cout << err << endl;
            return 1;
        }
        cout << "Mem copy ready" << endl;
        encode_to_disk(set.fileName.c_str(), output, unsigned(resizedImage.width), unsigned(resizedImage.height));
    }

    zncc.setArg(5, 4);
    zncc.setArg(6, 70);

    for (auto set : {left}) {
        zncc.setArg(0, left.gs);
        zncc.setArg(1, right.gs);
        zncc.setArg(2, left.meaned);
        zncc.setArg(3, right.meaned);
        zncc.setArg(4, left.znccd);

        cl::Event e1;
        int err = queue.enqueueNDRangeKernel(zncc, cl::NullRange,
                                             cl::NDRange(resizedImage.width, resizedImage.height), cl::NullRange, NULL,
                                             &e1);
         vector<uint8_t> output(resizedImage.height * resizedImage.width * 4);

        cl::size_t<3> start;
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;

        cl::size_t<3> end;
        end[0] = resizedImage.width;
        end[1] = resizedImage.height;
        end[2] = 1;

        e1.wait();
        try {
            err = queue.enqueueReadImage(set.znccd, CL_TRUE, start, end, 0, 0, &output[0], NULL, NULL);
        } catch (const cl::Error &ex) {
            std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
            return 1;
        }

        if (err != CL_SUCCESS) {
            cout << err << endl;
            return 1;
        }
        cout << "Mem copy ready" << endl;
        encode_to_disk((string("zncc_").append(set.fileName)).c_str(), output, unsigned(resizedImage.width), unsigned(resizedImage.height));
    }

    cout << "Bye" << endl;
}