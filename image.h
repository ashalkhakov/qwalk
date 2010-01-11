/*
    QShed <http://www.icculus.org/qshed>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IMAGE_H
#define IMAGE_H

typedef struct palette_s
{
	unsigned char rgb[768];
	unsigned int fullbright_flags[8];
} palette_t;

/* 32-bit image */
typedef struct image_rgba_s
{
	int width, height;
	unsigned char *pixels;
} image_rgba_t;

/* 8-bit (256 colour) paletted image */
typedef struct image_paletted_s
{
	palette_t palette;

	int width, height;
	unsigned char *pixels;
} image_paletted_t;

image_rgba_t *image_load(const char *filename, void *filedata, size_t filesize);

image_rgba_t *image_alloc(int width, int height);
void image_free(image_rgba_t **image);

image_rgba_t *image_createfill(int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
image_rgba_t *image_clone(const image_rgba_t *source);

image_rgba_t *image_pcx_load(void *filedata, size_t filesize);
image_rgba_t *image_tga_load(void *filedata, size_t filesize);
image_rgba_t *image_jpeg_load(void *filedata, size_t filesize);

size_t image_pcx_save(const image_paletted_t *image, void *filedata, size_t filesize);

image_paletted_t *image_palettize(const palette_t *palette, const image_rgba_t *source_diffuse, const image_rgba_t *source_fullbright);
image_rgba_t *image_pad(const image_rgba_t *source, int width, int height);
image_rgba_t *image_resize(const image_rgba_t *source, int width, int height);

void image_drawpixel(image_rgba_t *image, int x, int y, unsigned char r, unsigned char g, unsigned char b);
void image_drawline(image_rgba_t *image, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b);

#endif
