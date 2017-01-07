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
    cl::Image2D original, gs, meaned, znccd, crosschecked, output;
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
    cl::Image2D crossChecked, occlusionFilled;
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

        right.original = cl::Image2D(ctx, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, imageFormat, w,
                                     h, 0, &right.originalImage.pixels[0], &image_err);
        right.gs = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                               NULL, &image_err);
        right.meaned = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                   NULL, &image_err);
        right.znccd = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                  NULL, &image_err);

        crossChecked = cl::Image2D(ctx, CL_MEM_READ_WRITE, imageFormat, resizedImage.width, resizedImage.height, 0,
                                   NULL, &image_err);
        occlusionFilled = cl::Image2D(ctx, CL_MEM_WRITE_ONLY, imageFormat, resizedImage.width, resizedImage.height, 0,
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
    cl::Kernel crossCheck(program, "cross_check");
    cl::Kernel occlusionFill(program, "nearest_nonzero");

    try {
        resize.setArg(0, h);
        resize.setArg(1, w);
        resize.setArg(4, h * w);
        mean.setArg(2, 4);
    } catch (const cl::Error &ex) {
        std::cerr << ex.what() << " " << ex.err() << endl;
        return 1;
    }

    cl::size_t<3> start;
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;

    cl::size_t<3> end;
    end[0] = resizedImage.width;
    end[1] = resizedImage.height;
    end[2] = 1;

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
        cout << "Resizing ready" << endl;
        encode_to_disk(set.fileName.c_str(), output, unsigned(resizedImage.width), unsigned(resizedImage.height));
    }

    zncc.setArg(5, 4);

    for (int i = 0; i < 2; i++) {
        int disp = 70;
        int min_disp, max_disp;
        min_disp = i == 0 ? 0 : -disp;
        max_disp = i == 0 ? disp : 0;
        imageSet l = i == 0 ? left : right;
        imageSet r = i == 0 ? right : left;

        zncc.setArg(0, l.gs);
        zncc.setArg(1, r.gs);
        zncc.setArg(2, l.meaned);
        zncc.setArg(3, r.meaned);
        zncc.setArg(4, l.znccd);
        zncc.setArg(6, min_disp);
        zncc.setArg(7, max_disp);

        cl::Event e1;
        int err = queue.enqueueNDRangeKernel(zncc, cl::NullRange,
                                             cl::NDRange(resizedImage.width, resizedImage.height), cl::NullRange, NULL,
                                             &e1);
        vector<uint8_t> output(resizedImage.height * resizedImage.width * 4);

        e1.wait();
        try {
            err = queue.enqueueReadImage(l.znccd, CL_TRUE, start, end, 0, 0, &output[0], NULL, NULL);
        } catch (const cl::Error &ex) {
            std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
            return 1;
        }

        if (err != CL_SUCCESS) {
            cout << err << endl;
            return 1;
        }
        cout << "ZNCC " << (i ? "right" : "left") << " ready" << endl;
        encode_to_disk((string("zncc_").append(l.fileName)).c_str(), output,
                       unsigned(resizedImage.width), unsigned(resizedImage.height));
    }

    crossCheck.setArg(0, left.znccd);
    crossCheck.setArg(1, right.znccd);
    crossCheck.setArg(2, crossChecked);
    crossCheck.setArg(3, 8);
    crossCheck.setArg(4, 70);

    cl::Event e1;
    int err = queue.enqueueNDRangeKernel(crossCheck, cl::NullRange,
                                         cl::NDRange(resizedImage.width, resizedImage.height), cl::NullRange, NULL,
                                         &e1);


    vector<uint8_t> crosschecked(resizedImage.height * resizedImage.width * 4);

    e1.wait();
    try {
        err = queue.enqueueReadImage(crossChecked, CL_TRUE, start, end, 0, 0, &crosschecked[0], NULL, NULL);
    } catch (const cl::Error &ex) {
        std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
        return 1;
    }

    if (err != CL_SUCCESS) {
        cout << err << endl;
        return 1;
    }

    cout << "Cross check ready" << endl;
    encode_to_disk("cross_checked.png", crosschecked, unsigned(resizedImage.width), unsigned(resizedImage.height));

    occlusionFill.setArg(0, crossChecked);
    occlusionFill.setArg(1, occlusionFilled);

    cl::Event e2;
    err = queue.enqueueNDRangeKernel(occlusionFill, cl::NullRange,
                                     cl::NDRange(resizedImage.width, resizedImage.height), cl::NullRange, NULL,
                                     &e2);

    vector<uint8_t> output(resizedImage.height * resizedImage.width * 4);

    e2.wait();

    try {
        err = queue.enqueueReadImage(occlusionFilled, CL_TRUE, start, end, 0, 0, &crosschecked[0], NULL, NULL);
    } catch (const cl::Error &ex) {
        std::cerr << "Error " << ex.what() << " code " << ex.err() << endl;
        return 1;
    }

    if (err != CL_SUCCESS) {
        cout << err << endl;
        return 1;
    }

    cout << "Occlusion fill ready" << endl;
    encode_to_disk("ready.png", crosschecked, unsigned(resizedImage.width), unsigned(resizedImage.height));


    cout << "Bye" << endl;
}