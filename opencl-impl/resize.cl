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


__kernel void calculate_zncc(
        __read_only image2d_t left,
        __read_only image2d_t right,
        __read_only image2d_t left_mean,
        __read_only image2d_t right_mean,
        __write_only image2d_t output,
        int window_size,
        int max_disp
        ) {
        sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;
        int x = get_global_id(0);
        int y = get_global_id(1);
        int2 coord = {x, y};

        int l_mean = read_imageui(left_mean, sampler, coord).s0;
        float lower_left_sum = 0;
        uint best_disp = 0;
        float best_zncc = 0;

        for(int y1 = -window_size ; y1 <= window_size; y1++ ) {
            for (int x1 = -window_size ; x1 <= window_size ; x1++ ){
                int2 coord2 = {x+x1, y+y1};
                int pix_val = read_imageui(left, sampler, coord2).s0 - l_mean;
                lower_left_sum += pix_val*pix_val;
            }
        }

        for (int disp = 0; disp < max_disp; disp++) {
            float lower_right_sum = 0;
            long upper_sum = 0;
            for(int y2 = -window_size ; y2 <= window_size; y2++ ) {
                for (int x2 = -window_size ; x2 <= window_size ; x2++ ){
                    int2 coord2 = {x+x2, y+y2};
                    int l_pix_val = read_imageui(left, sampler, coord2).s0 - l_mean;
                    int2 coord3 = {x+x2-disp, y+y2};
                    uint r_mean = read_imageui(right_mean, sampler, coord3).s0;
                    int r_pix_val = read_imageui(right, sampler, coord3).s0 - r_mean;
                    lower_right_sum += r_pix_val*r_pix_val;
                    upper_sum += r_pix_val * l_pix_val;
                }
            }
            float zncc = upper_sum / (sqrt(lower_left_sum)*sqrt(lower_right_sum));
            if (zncc > best_zncc) {
                best_disp = disp;
                best_zncc = zncc;
            }
        }
        uint4 best_pix = {best_disp, best_disp, best_disp, 255};
        write_imageui(output, coord, best_pix);
}
