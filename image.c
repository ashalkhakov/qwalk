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

#include <stdio.h>
#include <string.h>

#include "global.h"
#include "image.h"

bool_t image_load(const char *filename, void *filedata, size_t filesize, image_rgba_t *out_image)
{
	const char *ext = strrchr(filename, '.');

	if (!ext)
	{
		printf("%s: no file extension\n", filename);
		return false;
	}

	if (!strncasecmp(ext, ".tga", 4))
		return image_tga_load(filedata, filesize, out_image);
	if (!strncasecmp(ext, ".jpg", 4) || !strncasecmp(ext, ".jpeg", 5))
		return image_jpeg_load(filedata, filesize, out_image);

	printf("unrecognized file extension \"%s\"\n", ext);
	return false;
}

void image_free(image_rgba_t *image)
{
	qfree(image->pixels);
}

bool_t image_clone(image_rgba_t *dest, const image_rgba_t *source)
{
	dest->width = source->width;
	dest->height = source->height;
	dest->pixels = (unsigned char*)qmalloc(dest->width * dest->height * 4);
	if (!dest->pixels)
		return false;
	memcpy(dest->pixels, source->pixels, dest->width * dest->height * 4);
	return true;
}

unsigned char palettize_colour(const unsigned char palette[768], unsigned char r, unsigned char g, unsigned char b)
{
	int i, dist;
	int besti = -1, bestdist;

	for (i = 0; i < 256 - 32; i++) /* ignore the fullbrights (FIXME - this is a hack, the fullbrights might be somewhere else) */
	{
		dist = abs(palette[i*3+0] - r) + abs(palette[i*3+1] - g) + abs(palette[i*3+2] - b);

		if (besti == -1 || dist < bestdist)
		{
			besti = i;
			bestdist = dist;
		}
	}

	return besti;
}

void palettize_image(const image_rgba_t *image_rgba, const unsigned char palette[768], image_paletted_t *out_image_paletted)
{
	int i;
	unsigned char *in, *out;

	out_image_paletted->width = image_rgba->width;
	out_image_paletted->height = image_rgba->height;
	out_image_paletted->pixels = (unsigned char*)qmalloc(out_image_paletted->width * out_image_paletted->height);

	in = image_rgba->pixels;
	out = out_image_paletted->pixels;
	for (i = 0; i < out_image_paletted->width * out_image_paletted->height; i++, in += 4, out++)
		*out = palettize_colour(palette, in[0], in[1], in[2]);
}

/* FIXME - incomplete - will fail if you try to make an image bigger */
void resize_image(image_rgba_t *image_rgba, int width, int height)
{
	int x, y;
	unsigned char *pixels = (unsigned char*)qmalloc(width * height * 4);

	for (y = 0; y < height; y++)
	{
		unsigned char *out = pixels + y * width * 4;
		int y1, y2, yy;

		yy = ((y * image_rgba->height + image_rgba->height / 2) / height);
		y1 = (((y - 1) * image_rgba->height + image_rgba->height / 2) / height); if (y1 < 0) y1 = 0;
		y2 = (((y + 1) * image_rgba->height + image_rgba->height / 2) / height); if (y1 > image_rgba->height - 1) y1 = image_rgba->height - 1;

		for (x = 0; x < width; x++)
		{
			int x1, x2, xx;
			int i, j;
			int colour[4];
			int num;

			xx = ((x * image_rgba->width + image_rgba->width / 2) / width);
			x1 = (((x - 1) * image_rgba->width + image_rgba->width / 2) / width); if (x1 < 0) x1 = 0;
			x2 = (((x + 1) * image_rgba->width + image_rgba->width / 2) / width); if (x1 > image_rgba->width - 1) x1 = image_rgba->width - 1;

			colour[0] = 0;
			colour[1] = 0;
			colour[2] = 0;
			colour[3] = 0;
			num = 0;

			for (j = y1; j <= y2; j++)
			{
				const unsigned char *in = image_rgba->pixels + j * image_rgba->width * 4;
				int dist;
				int jdist;
				
				if (j < yy)
					jdist = (yy - y1) - (yy - j);
				else if (j > yy)
					jdist = (y2 - yy) - (j - yy);
				else
					jdist = max(y2 - yy, yy - y1);

				for (i = x1; i <= x2; i++)
				{
					int idist;

					if (i < xx)
						idist = (xx - x1) - (xx - i);
					else if (i > xx)
						idist = (x2 - xx) - (i - xx);
					else
						idist = max(x2 - xx, xx - x1);

					dist = idist * jdist;

					colour[0] += dist * in[i*4+0];
					colour[1] += dist * in[i*4+1];
					colour[2] += dist * in[i*4+2];
					colour[3] += dist * in[i*4+3];
					num += dist;
				}
			}

			if (num > 0) /* i think this value is 0 if the new width is the same or more than the old width? */
			{
				out[x*4+0] = colour[0] / num;
				out[x*4+1] = colour[1] / num;
				out[x*4+2] = colour[2] / num;
				out[x*4+3] = colour[3] / num;
			}
			else
			{
				out[x*4+0] = out[x*4+1] = out[x*4+2] = out[x*4+3] = 0;
			}
		}
	}

	qfree(image_rgba->pixels);

	image_rgba->width = width;
	image_rgba->height = height;
	image_rgba->pixels = pixels;
}

bool_t pad_image(image_rgba_t *out_image_rgba, const image_rgba_t *in_image_rgba, int width, int height)
{
	unsigned char *pixels;
	int x, y;

	if (width < in_image_rgba->width || height < in_image_rgba->height)
		return false;

	pixels = (unsigned char*)qmalloc(width * height * 4);

/* FIXME - implement this properly later (no redundant overwriting, stretch edge pixels instead of filling with black) */
	memset(pixels, 0, width * height * 4);
	for (y = 0; y < in_image_rgba->height; y++)
	{
		for (x = 0; x < in_image_rgba->width; x++)
		{
			pixels[(y*width+x)*4+0] = in_image_rgba->pixels[(y*in_image_rgba->width+x)*4+0];
			pixels[(y*width+x)*4+1] = in_image_rgba->pixels[(y*in_image_rgba->width+x)*4+1];
			pixels[(y*width+x)*4+2] = in_image_rgba->pixels[(y*in_image_rgba->width+x)*4+2];
			pixels[(y*width+x)*4+3] = in_image_rgba->pixels[(y*in_image_rgba->width+x)*4+3];
		}
	}

	out_image_rgba->width = width;
	out_image_rgba->height = height;
	out_image_rgba->pixels = pixels;
	return true;
}

bool_t clone_image(image_rgba_t *out_image_rgba, const image_rgba_t *in_image_rgba)
{
	unsigned char *pixels = (unsigned char*)qmalloc(in_image_rgba->width * in_image_rgba->height * 4);
	if (!pixels)
		return false;
	memcpy(pixels, in_image_rgba->pixels, in_image_rgba->width * in_image_rgba->height * 4);
	out_image_rgba->width = in_image_rgba->width;
	out_image_rgba->height = in_image_rgba->height;
	out_image_rgba->pixels = pixels;
	return true;
}
