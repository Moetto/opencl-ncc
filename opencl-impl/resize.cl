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
        uint gs = (uint) (pixel.s0 * 0.2126 + pixel.s1 * 0.7152 + pixel.s2 * 0.0722);
        uint4 gs_pixel = {gs, gs, gs, 255};
        write_imageui(resized, coord2, gs_pixel);
    }
}


__kernel void calculate_mean(
        __read_only image2d_t input,
        __write_only image2d_t output,
        int window_size) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    int2 coord = {x, y};
    sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;

    ulong mean = 0;
    for (int y1 = - window_size; y1 <= window_size ; y1++) {
        for (int x1 = -window_size ; x1 <= window_size ; x1++) {
            int2 coord2 = {x+x1, y+y1};
            mean += read_imageui(input, sampler, coord2).s0;
        }
    }

    uint val = (uint) (mean / ((2*window_size+1)*(2*window_size+1)));
    uint4 pix = {val, val, val, 255};
    write_imageui(output, coord, pix);
}
