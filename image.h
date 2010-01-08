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

/* 32-bit image */
typedef struct image_rgba_s
{
	int width, height;
	unsigned char *pixels;
} image_rgba_t;

/* 8-bit (256 colour) paletted image */
typedef struct image_paletted_s
{
	int width, height;
	unsigned char *pixels;
} image_paletted_t;

bool_t image_load(const char *filename, void *filedata, size_t filesize, image_rgba_t *out_image);
void image_free(image_rgba_t *image);

bool_t image_createfill(image_rgba_t *out_image_rgba, int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
bool_t image_clone(image_rgba_t *dest, const image_rgba_t *source);

bool_t image_pcx_load(void *filedata, size_t filesize, image_rgba_t *out_image);
bool_t image_tga_load(void *filedata, size_t filesize, image_rgba_t *out_image);
bool_t image_jpeg_load(void *filedata, size_t filesize, image_rgba_t *out_image);

void image_palettize(const image_rgba_t *image_diffuse, const image_rgba_t *image_fullbright, const unsigned char palette[768], image_paletted_t *out_image_paletted);
bool_t image_pad(image_rgba_t *out_image_rgba, const image_rgba_t *in_image_rgba, int width, int height);
void image_resize(image_rgba_t *image_rgba, int width, int height);

void image_drawpixel(image_rgba_t *image, int x, int y, unsigned char r, unsigned char g, unsigned char b);
void image_drawline(image_rgba_t *image, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b);

#endif
