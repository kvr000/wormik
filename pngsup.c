#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <png.h>

int read_true_png(const char *file, unsigned *width_, unsigned *height_, unsigned *depth_, void **data)
{
    FILE *fpin;
    png_structp read_ptr;
    png_infop read_info_ptr;
    png_uint_32 width, height;
    int bit_depth, color_type;
    unsigned i;
    
    if (!(fpin = fopen(file, "rb"))) {
	fprintf(stderr, "failed to open input file %s: %s\n", file, strerror(errno));
    }

    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    read_info_ptr = png_create_info_struct(read_ptr);
    //end_info_ptr = png_create_info_struct(read_ptr);

    if (setjmp(png_jmpbuf(read_ptr))) {
	png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
	fclose(fpin);
	return -1;
    }
    png_init_io(read_ptr, fpin);
    png_set_read_status_fn(read_ptr, NULL);
    png_read_info(read_ptr, read_info_ptr);
    {
	int interlace_type, compression_type, filter_type;
    png_get_IHDR(read_ptr, read_info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_type);
    }
    if (bit_depth != 8) {
	fprintf(stderr, "png: unexpected bitdepth: %d\n", bit_depth);
	longjmp(png_jmpbuf(read_ptr), 1);
    }
    if (color_type != PNG_COLOR_TYPE_RGB) {
	fprintf(stderr, "png: unexpected color_type: %d\n", color_type);
	longjmp(png_jmpbuf(read_ptr), 1);
    }
    *data = malloc(height*png_get_rowbytes(read_ptr, read_info_ptr));
    for (i = 0; i < height; i++) {
	png_bytep *targ = (png_bytep *)((char *)*data+i*png_get_rowbytes(read_ptr, read_info_ptr));
	png_read_rows(read_ptr, (png_bytepp)&targ, NULL, 1);
    }
    png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
    fclose(fpin);
    *width_ = width;
    *height_ = height;
    *depth_ = bit_depth;
    return 0;
}
