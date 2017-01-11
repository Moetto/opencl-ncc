# OpenCL stereo disparity algorithm - Multiprocessor programming exercise
We have implemented an algorithm for creating a depth map using two images taken of the same scene with an offset in the horizontal axis. The program was originally created in C++, then ported to use OpenCL bindings to perform computations a desktop/laptop GPU, and finally the OpenCL code was adapted to be run on the Odroid-XU4 embedded system.

The algorithm consists of three distinct parts: Preprocessing, wherein the input images are resized and greyscaled; The algorithm itself, where a disparity map is calculated by using zero-mean normalized cross-correlation (ZNCC) as the likeness criteria; and the post-processing, where the disparity maps of both images are cross-checked and the non-correlating

## Preprocessing
In all implementations, the images are resized by taking a pixel from every 4th row and every 4th column, which is converted from RGBA values to 8-bit integer values, and outputting it to a smaller image. This resized greyscale image is used for further steps.

The greyscale values are calculated as Y=0.2126R+0.7152G+0.0722B

## Disparity algorithm
The disparity algorithm is implemented largely as the provided pseudocode describes, except for the window mean values. In the C++ implementation, the mean of each window is calculated within the disparity value loop, except for the "left" pixel which can be calculated before the loop. In the OpenCL implementations, however, the window means of each pixels are calculated beforehand in a separate step, and used as input for the disparity algorithm. This is done
to avoid calculating the window mean every time several times (up to MAX_DISP*2 times, e.g. about 100-150).

The disparity algorithm is applied first with a disparity range of 0..MAX_DISP, and then with the range -MAX_DISP..0 with the image inputs swapped, wherein the second iteration calculates

## Post-processing
The post processing is performed in two steps: cross-check and occlusion fill.

Cross-check is performed by comparing corresponding between the two disparity images, and outputting the value from the left-hand disparity image if their difference is smaller than or equal to a given threshold, and 0 otherwise. Simultaneously, the pixel value is scaled to the range 0..255 from 0..MAX_DISP to produce a more appealing end result.

The occlusion takes the single image produced by the cross-check and outputs for each pixel the nearest non-zero pixel (itself if applicable), using euclidean distance as nearness criteria.

The final output is written to disk after the occlusion fill.

## OpenCL details
The OpenCL implementation consists of 5 kernels: `resize`, `calculate_mean`, `calculate_zncc`, `cross_check` and `nearest_nonzero`.

### resize
This kernel uses a range of x=0..(width/4)-1 and y=0..(height/4)-1, for each pixel in the output image. Images are read and written using OpenCL `image` objects in global memory. There is no need to use local memory as reads and writes are performed for only one pixel per work-group.

### calculate_mean

### calculate_zncc

### cross-check
The cross-check kernel reads from both `image2d_t`s in global memory and outputs to one `image2d_t`. Local memory would not help as each pixel is only accessed once.

### nearest_nonzero
This kernel performs the occlusion fill. It reads and writes ´image´ values from global memory. Local memory could possibly be used as surrounding pixels are accessed, but as the access pattern is somewhat unpredictable and the potential optimization insignificant compared to `calculate_zncc`, this optimization was not performed.

## Execution times
### Hardware (C++ and OpenCL implementations):
* CPU:
* GPU:
* RAM:
* OpenCL version:

### Total time:
* C++           xx.xxxxxxx s
* OpenCL        xx.xxxxxxx s
* Optimized     xx.xxxxxxx s
* Odroid        xx.xxxxxxx s

### Preprocessing:
* C++           xx.xxxxxxx s
* OpenCL        xx.xxxxxxx s
* Optimized     xx.xxxxxxx s
* Odroid        xx.xxxxxxx s

### Disparity algorithm:
* C++           xx.xxxxxxx s
* OpenCL        xx.xxxxxxx s
* Optimized     xx.xxxxxxx s
* Odroid        xx.xxxxxxx s

### Cross-check:
* C++           xx.xxxxxxx s
* OpenCL        xx.xxxxxxx s
* Optimized     xx.xxxxxxx s
* Odroid        xx.xxxxxxx s

### Occlusion fill:
* C++           xx.xxxxxxx s
* OpenCL        xx.xxxxxxx s
* Optimized     xx.xxxxxxx s
* Odroid        xx.xxxxxxx s
