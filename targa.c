/*
 * Targa file support for 3D Studio converter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include "internal.h"


/*
 * open a .TGA file, and fill in the appropriate fields
 * (twidth, theight, red, green, and blue) of the
 * corresponding material structure
 *
 * Parameters:
 *	mat:		pointer to the material
 *	colr:		0 if the color fields are not to be manipulated
 *			1 if the average texture color is to be
 *			  filled in
 * Global variables:
 *	filepath:	string giving the path where the .3ds file
 *			lives
 * Returns:
 * 0 on success
 * -1 if unable to read the .TGA file
 *    in the latter case, an error message is printed here
 */

extern char *filepath;

typedef struct pixel {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} Pixel;

/* State info for reading RLE-coded pixels; both counts must be init to 0 */
static int block_count;		/* # of pixels remaining in RLE block */
static int dup_pixel_count;	/* # of times to duplicate previous pixel */

static char tga_pixel[4];

/*
 * read_rle_pixel: read a pixel from an RLE encoded .TGA file
 */
static void
read_rle_pixel(FILE *fhandle, Pixel *place)
{
	int i;

	/* if we're in the middle of reading a duplicate pixel */
	if (dup_pixel_count > 0) {
		dup_pixel_count--;
		place->blue = tga_pixel[0];
		place->green = tga_pixel[1];
		place->red = tga_pixel[2];
		return;
	}
	/* should we read an RLE block header? */
	if (--block_count < 0) {
		i = fgetc(fhandle);
		if (i < 0) return;			/* end of file */
		if (i & 0x80) {
			dup_pixel_count = i & 0x7f;	/* number of duplications after this one */
			block_count = 0;		/* then a new block header */
		} else {
			block_count = i & 0x7f;		/* this many unduplicated pixels */
		}
	}
	place->blue = tga_pixel[0] = fgetc(fhandle);
	place->green = tga_pixel[1] = fgetc(fhandle);
	place->red = tga_pixel[2] = fgetc(fhandle);
}

/*
 * read a pixel from an uncompressed .TGA file
 */
static void
read_norm_pixel(FILE *fhandle, Pixel *place)
{
	place->blue = fgetc(fhandle);
	place->green = fgetc(fhandle);
	place->red = fgetc(fhandle);
}

int
read_targa( Material *mat, int colrflag )
{
	uint32_t red, green, blue;
	uint32_t numpixels;
	int bytes_in_name;
	int cmap_type;
	int sub_type;
	int bits_per_pixel;
	void (*read_pixel)(FILE *fhandle, Pixel *place);
	Pixel pix;

	unsigned int image_w;			/* width of image in pixels from TGA header */
	unsigned int image_h;			/* height of image in pixels from TGA header */
	FILE *fhandle;
	int c, i;
	static char infile[256];

	strcpy(infile, filepath);
	strcat(infile, mat->texmap);

	fhandle = fopen(infile, "rb");
	if (!fhandle) {
#ifndef _WIN32
		/* Do a case insensitive search for the file */
		DIR *d = opendir(strlen(filepath) ? filepath : ".");
		if (d) {
			struct dirent *de;
			for (de = readdir(d); de; de = readdir(d)) {
				if (strcasecmp(mat->texmap, de->d_name)) continue;
				/* Match. Reconstruct infile and retry open. */
				strcpy(infile, filepath);
				strcat(infile, de->d_name);
				fhandle = fopen(infile, "rb");
				break;
			}
			closedir(d);
		}
#endif
		if (!fhandle) {
			perror(infile);
			return -1;
		}
	}

	bytes_in_name = fgetc(fhandle);
	cmap_type = fgetc(fhandle);
	sub_type = fgetc(fhandle);
	c = fgetc(fhandle);			/* skip bytes 3 and 4 */
	c = fgetc(fhandle);

	c = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);	/* color map length */
	c = fgetc(fhandle);			/* skip bytes 7 through 11 */
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	c = fgetc(fhandle);
	if (c < 0) {
		fprintf(stderr, "Unexpected end of file on %s\n", infile);
		fclose(fhandle);
		return -1;
	}
	image_w = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);
	image_h = fgetc(fhandle) + ((unsigned)fgetc(fhandle) << 8);

	mat->twidth = image_w;
	mat->theight = image_h;
	if (!colrflag) {
		fclose(fhandle);
		return 0;
	}

	/* read all the pixels in the file */

	bits_per_pixel = fgetc(fhandle);
	(void)fgetc(fhandle);		/* skip tga flags */

	if (cmap_type != 0 || bits_per_pixel != 24) {
		fprintf(stderr, "WARNING: %s is not a 24 bit Targa file; using default color\n", infile);
		fclose(fhandle);
		return -1;
	}

/* figure out how to read source pixels */
	if (sub_type > 8) {
	/* an RLE-coded file */
		block_count = 0;
		dup_pixel_count = 0;
		read_pixel = read_rle_pixel;
		sub_type -= 8;
	} else {
		read_pixel = read_norm_pixel;
	}

	if (sub_type == 1) {
		fprintf(stderr, "WARNING: %s is not a 24 bit Targa file; using default color\n", infile);
		fclose(fhandle);
		return -1;
	} else if (sub_type == 2) {
		/* everything is OK */ ;
	} else {
		fprintf(stderr, "WARNING: %s is an invalid or unsupported Targa file\n", infile);
		fclose(fhandle);
		return -1;
	}

/* skip the image name */
	for (i = 0; i < bytes_in_name; i++) {
		c = fgetc(fhandle);
		if (c < 0) {
			fprintf(stderr, "WARNING: unexpected end of file on %s\n", infile);
			fclose(fhandle);
			return -1;
		}
	}

	numpixels = image_w * (uint32_t)image_h;
	red = green = blue = 0;
	while (numpixels > 0) {
		read_pixel(fhandle, &pix);
		red += pix.red;
		blue += pix.blue;
		green += pix.green;
		--numpixels;
	}
	numpixels = image_w * (uint32_t)image_h;

	mat->red = red/numpixels;
	mat->green = green/numpixels;
	mat->blue = blue/numpixels;
	fclose(fhandle);
	return 0;
}
