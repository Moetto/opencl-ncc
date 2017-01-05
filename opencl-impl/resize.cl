__kernel void resize(
        const long height,
        const long width,
        __read_only image2d_t original,
        __write_only image2d_t resized,
        const long image_size) {

    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x % 4 == 0 && y % 4 == 0) {
        int2 coord = {x, y};
        int2 coord2 = {x / 4, y / 4};
        sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_LINEAR;
        uint4 pixel = read_imageui(original, sampler, coord);
        write_imageui(resized, coord2, pixel);
    }
}