__kernel void resize(
        const long height,
        const long width,
        __read_only image2d_t original,
        __write_only image2d_t resized,
        const long image_size) {

    int2 coord = (get_global_id(0), get_global_id(1));

    //if (height % 2 == 0) {
    write_imageui(resized, coord, (0, 0, 0, 0));
    //}
}
