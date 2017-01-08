__kernel void resize(
        const long height,
        const long width,
        __read_only image2d_t original,
        __write_only image2d_t resized,
        const long image_size) {

    int x = get_global_id(0);
    int y = get_global_id(1);
    int2 coord = {x*4, y*4};
    int2 coord2 = {x, y};
    sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_LINEAR;
    uint4 pixel = read_imageui(original, sampler, coord);
    uint gs = (uint) (pixel.s0 * 0.2126 + pixel.s1 * 0.7152 + pixel.s2 * 0.0722);
    uint4 gs_pixel = {gs, gs, gs, 255};
    write_imageui(resized, coord2, gs_pixel);
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
        int min_disp,
        int max_disp
        ) {
        sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;
        int x = get_global_id(0);
        int y = get_global_id(1);
        int2 coord = {x, y};

        int l_mean = read_imageui(left_mean, sampler, coord).s0;
        float lower_left_sum = 0;
        int best_disp = min_disp;
        float best_zncc = 0;

        for(int y1 = -window_size ; y1 <= window_size; y1++ ) {
            for (int x1 = -window_size ; x1 <= window_size ; x1++ ){
                int2 coord2 = {x+x1, y+y1};
                int pix_val = read_imageui(left, sampler, coord2).s0 - l_mean;
                lower_left_sum += pix_val*pix_val;
            }
        }

        for (int disp = min_disp ; disp < max_disp ; disp++) {
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
        best_disp = abs(best_disp);
        uint4 best_pix = {best_disp, best_disp, best_disp, 255};
        write_imageui(output, coord, best_pix);
}

__kernel void cross_check(
    __read_only image2d_t left,
    __read_only image2d_t right,
    __write_only image2d_t output,
    uint threshold,
    uint max_disp
    ) {
    int x = get_global_id(0);
    int y = get_global_id(1);

    int2 coord = {x,y};

    sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_LINEAR;
    uint l = read_imageui(left, sampler, coord).s0;
    uint r = read_imageui(right, sampler, coord).s0;

    if(abs(l-r) < threshold){
        l = l * 255 / max_disp;
        uint4 pix = {l, l, l, 255};
        write_imageui(output, coord, pix);
    } else {
        uint4 pix = {0, 0, 0, 255};
        write_imageui(output, coord, pix);
    }
}

/* Finds the nearest nonzero pixel by extending a ring around the current pixel
 *
 */
__kernel void nearest_nonzero(
    __read_only image2d_t input,
    __write_only image2d_t output
    ) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    int2 coord={x, y};

    float closest_dist = -1;
    uint closest_value = 0;

    sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_LINEAR;

    for (int offset = 0; offset < 100; offset++) {
        if (closest_dist >= 0 && closest_dist <= offset) {
            // We can no longer find a closer pixel within the current offset
            break;
        }
        for (int xsign = -offset; xsign <= offset; xsign++) {
            for (int ysign = -offset; ysign <= offset; ysign++) {
                if (abs(xsign) < offset && abs(ysign) < offset) {
                    // Don't loop the same pixels again
                    continue;
                }

                int2 coord2 = {x+xsign,y + ysign};
                uint pixel = read_imageui(input, sampler, coord2).s0;
                if (pixel > 0) {
                    float dist = sqrt((float)(xsign * xsign + ysign * ysign));
                    if (closest_dist < 0 || closest_dist > dist) {
                        closest_dist = dist;
                        closest_value = pixel;
                    }
                }
            }
        }
    }

    uint4 pix = {closest_value, closest_value, closest_value, 255};
    write_imageui(output, coord, pix);
}

