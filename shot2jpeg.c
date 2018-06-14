// screenshot to jpeg for linux

#include <stdio.h>
#include <sys/time.h>
#include "shot2jpeg.h"

#define IMG_AT(x, y, i) image->data[(((y_width) + (x)) << 2) + (i)]
#define RGBA_AT(x, y, i) rgba[(((y_width) + (x)) << 2) + (i)]
#define RGB_AT(x, y, i) rgb[((y_width) + (x)) * 3 + (i)]

xcb_image_t *take_screenshot(xcb_connection_t *conn, xcb_screen_t *screen) {
    // enum xcb_image_format_t { XCB_IMAGE_FORMAT_XY_BITMAP = 0, XCB_IMAGE_FORMAT_XY_PIXMAP = 1, XCB_IMAGE_FORMAT_Z_PIXMAP = 2 }
    return xcb_image_get(conn,
        screen->root,
        0, 0,
        screen->width_in_pixels, screen->height_in_pixels,
        UINT32_MAX,
        XCB_IMAGE_FORMAT_Z_PIXMAP);
}

xcb_pixmap_t image_to_pixmap(xcb_connection_t *conn, xcb_screen_t *screen, xcb_image_t *image) {
    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, 24, pixmap, screen->root, image->width, image->height);

    xcb_gcontext_t gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, pixmap,
        XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
        (uint32_t[]){ screen->black_pixel, 0xffffff });

    xcb_image_put(conn, pixmap, gc, image, 0, 0, 0);

    return pixmap; // remember to free the memory: xcb_free_pixmap(conn, pixmap);
}

void get_rgba_image_data(xcb_image_t *image, uint8_t *rgba) {
    for (int y = 0; y < image->height; y++) {
        int y_width = y*image->width;
        for (int x = 0; x < image->width; x++) {
            RGBA_AT(x, y, 0) = IMG_AT(x, y, 2); // r
            RGBA_AT(x, y, 1) = IMG_AT(x, y, 1); // g
            RGBA_AT(x, y, 2) = IMG_AT(x, y, 0); // b
            RGBA_AT(x, y, 3) = IMG_AT(x, y, 3); // a
        }
    }
}

void get_rgba_image_data2(xcb_image_t *image, uint8_t *rgba) {
    memcpy(rgba, image->data, image->size);
    for (int y = 0; y < image->height; y++) {
        int y_width = y*image->width;
        for (int x = 0; x < image->width; x++) {
            RGBA_AT(x, y, 0) = IMG_AT(x, y, 2); // r
            RGBA_AT(x, y, 2) = IMG_AT(x, y, 0); // b
        }
    }
}

void get_rgba_image_data3(xcb_image_t *image) {
    for (int y = 0; y < image->height; y++) {
        int y_width = y*image->width;
        for (int x = 0; x < image->width; x++) {
            IMG_AT(x, y, 0) = IMG_AT(x, y, 0) ^ IMG_AT(x, y, 2); // r
            IMG_AT(x, y, 2) = IMG_AT(x, y, 2) ^ IMG_AT(x, y, 0); // b
            IMG_AT(x, y, 0) = IMG_AT(x, y, 0) ^ IMG_AT(x, y, 2);
        }
    }
}

void get_rgba_image_data4(xcb_image_t *image) {
    uint8_t *rgba = image->data, tmp;
    int height = image->height, width = image->width, y_width = -width, x, y;
    for (y = 0; y < height; y++) {
        y_width += width;
        for (x = 0; x < width; x++) {
            tmp = RGBA_AT(x, y, 0);
            RGBA_AT(x, y, 0) = RGBA_AT(x, y, 2);
            RGBA_AT(x, y, 2) = tmp;
        }
    }
}

void get_rgba_image_data5(xcb_image_t *image) {
    uint8_t *rgba = image->data, tmp;
    int length = image->height * image->width * 4, i;
    for (i = 0; i < length; i+=4) {
        tmp = rgba[i];
        rgba[i] = rgba[i + 2];
        rgba[i + 2] = tmp;
    }
}

void get_rgb_image_data(xcb_image_t *image, uint8_t *rgb) {
    for (int y = 0; y < image->height; y++) {
        int y_width = y*image->width;
        for (int x = 0; x < image->width; x++) {
            RGB_AT(x, y, 0) = IMG_AT(x, y, 2); // r
            RGB_AT(x, y, 1) = IMG_AT(x, y, 1); // g
            RGB_AT(x, y, 2) = IMG_AT(x, y, 0); // b
        }
    }
}

void write_to_jpeg(char *filename, int quality, xcb_image_t *image) {
    uint8_t data[image->width*image->height*4];
    get_rgba_image_data2(image, data);
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    cinfo.input_components = 4;
    cinfo.in_color_space = getJCS_EXT_RGBA();
    if (cinfo.in_color_space == JCS_UNKNOWN) {
        fprintf(stderr, "JCS_EXT_RGBA is not supported (probably built without libjpeg-trubo)");
        exit(1);
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image->width * 4;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &data[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
}

void write_to_jpeg_buffer(FILE *stream, int quality, xcb_image_t *image) {
    // struct timeval t, tt, ttt, tttt;
    uint8_t *data;
    // gettimeofday(&t, NULL);
    data = image->data;
    // gettimeofday(&tt, NULL);
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, stream);

    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    cinfo.input_components = 4;
    cinfo.in_color_space = getJCS_EXT_RGBA();
    if (cinfo.in_color_space == JCS_UNKNOWN) {
        fprintf(stderr, "JCS_EXT_RGBA is not supported (probably built without libjpeg-trubo)");
        exit(1);
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image->width * 4;

    // gettimeofday(&ttt, NULL);
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &data[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    // gettimeofday(&tttt, NULL);
    // float get_time = ((tt.tv_sec - t.tv_sec) * 1000000 + (tt.tv_usec - t.tv_usec)) / 1000;
    // float write_time = ((tttt.tv_sec - ttt.tv_sec) * 1000000 + (tttt.tv_usec - ttt.tv_usec)) / 1000;
    // printf(">>>>>> get_time: %.3f, write_time: %.3fms\n", get_time, write_time);

    jpeg_finish_compress(&cinfo);
    fclose(stream);
    jpeg_destroy_compress(&cinfo);
}