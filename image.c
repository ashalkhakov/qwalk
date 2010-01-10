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

	if (!strncasecmp(ext, ".pcx", 4))
		return image_pcx_load(filedata, filesize, out_image);
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

bool_t image_createfill(image_rgba_t *out_image_rgba, int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int i;
	unsigned char *pixels = (unsigned char*)qmalloc(width * height * 4);
	if (!pixels)
		return false;
	for (i = 0; i < width * height; i++)
	{
		pixels[i*4+0] = r;
		pixels[i*4+1] = g;
		pixels[i*4+2] = b;
		pixels[i*4+3] = a;
	}
	out_image_rgba->width = width;
	out_image_rgba->height = height;
	out_image_rgba->pixels = pixels;
	return true;
}

bool_t image_clone(image_rgba_t *out_image_rgba, const image_rgba_t *in_image_rgba)
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

static unsigned char palettize_colour(const palette_t *palette, bool_t fullbright, unsigned char r, unsigned char g, unsigned char b)
{
	int i, dist;
	int besti = -1, bestdist;

	for (i = 0; i < 256; i++)
	{
		if (fullbright != !!(palette->fullbright_flags[i >> 5] & (1U << (i & 31))))
			continue;

		dist = abs(palette->rgb[i*3+0] - r) + abs(palette->rgb[i*3+1] - g) + abs(palette->rgb[i*3+2] - b);

		if (besti == -1 || dist < bestdist)
		{
			besti = i;
			bestdist = dist;
		}
	}

	return (besti != -1) ? besti : 0;
}

void image_palettize(const image_rgba_t *image_diffuse, const image_rgba_t *image_fullbright, const palette_t *palette, image_paletted_t *out_image_paletted)
{
	int i;
	const unsigned char *in_diffuse, *in_fullbright;
	unsigned char *out;

	/*out_image_paletted->palette = *palette;*/
	out_image_paletted->width = image_diffuse->width;
	out_image_paletted->height = image_diffuse->height;
	out_image_paletted->pixels = (unsigned char*)qmalloc(out_image_paletted->width * out_image_paletted->height);

	in_diffuse = image_diffuse->pixels;
	in_fullbright = image_fullbright->pixels;
	out = out_image_paletted->pixels;
	for (i = 0; i < out_image_paletted->width * out_image_paletted->height; i++, in_diffuse += 4, in_fullbright += 4, out++)
	{
		if (in_fullbright[0] || in_fullbright[1] || in_fullbright[2])
			*out = palettize_colour(palette, true, in_fullbright[0], in_fullbright[1], in_fullbright[2]);
		else
			*out = palettize_colour(palette, false, in_diffuse[0], in_diffuse[1], in_diffuse[2]);
	}
}

/* FIXME - incomplete - will fail if you try to make an image bigger */
void image_resize(image_rgba_t *image_rgba, int width, int height)
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

/* pad an image to a larger size. the edge pixels will be repeated instead of filled with black, to avoid any unwanted
 * bleeding if mipmapped and/or rendered with texture filtering. the in and out images can be the same. */
bool_t image_pad(image_rgba_t *out_image_rgba, const image_rgba_t *in_image_rgba, int width, int height)
{
	const unsigned char *inpixels;
	unsigned char *outpixels, *outp;
	int x, y;

	if (width < in_image_rgba->width || height < in_image_rgba->height)
		return false;

	inpixels = in_image_rgba->pixels;
	outpixels = (unsigned char*)qmalloc(width * height * 4);
	outp = outpixels;

	for (y = 0; y < in_image_rgba->height; y++)
	{
		memcpy(outp, inpixels, in_image_rgba->width * 4);
		outp += in_image_rgba->width * 4;
		inpixels += in_image_rgba->width * 4;

		for (x = in_image_rgba->width; x < width; x++, outp += 4)
		{
			outp[0] = outp[-4];
			outp[1] = outp[-3];
			outp[2] = outp[-2];
			outp[3] = outp[-1];
		}
	}
	for (; y < height; y++)
	{
		memcpy(outp, outp - width * 4, width * 4);
		outp += width * 4;
	}

	if (out_image_rgba == in_image_rgba)
		qfree(out_image_rgba->pixels);

	out_image_rgba->width = width;
	out_image_rgba->height = height;
	out_image_rgba->pixels = outpixels;
	return true;
}

void image_drawpixel(image_rgba_t *image, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char *ptr;
	if (x < 0 || x >= image->width)
		return;
	if (y < 0 || y >= image->height)
		return;
	ptr = image->pixels + (y * image->width + x) * 4;
	ptr[0] = r;
	ptr[1] = g;
	ptr[2] = b;
	ptr[3] = 255;
}

void image_drawline(image_rgba_t *image, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b)
{
	int i, dx, dy, dxabs, dyabs, sdx, sdy, px, py;

	if (x1 < 0 && x2 < 0) return;
	if (y1 < 0 && y2 < 0) return;
	if (x1 >= image->width && x2 >= image->width) return;
	if (y1 >= image->height && y2 >= image->height) return;

	dx = x2 - x1;
	dy = y2 - y1;

	dxabs = abs(dx);
	dyabs = abs(dy);

	sdx = (dx < 0) ? -1 : ((dx > 0) ? 1 : 0);
	sdy = (dy < 0) ? -1 : ((dy > 0) ? 1 : 0);

	px = x1;
	py = y1;

	image_drawpixel(image, px, py, r, g, b);

	if (dxabs >= dyabs) /* line is more horizontal than vertical */
	{
		int y = dxabs >> 1;

		for (i = 0; i < dxabs; i++)
		{
			y += dyabs;
			if (y >= dxabs)
			{
				y -= dxabs;
				py += sdy;
			}
			px += sdx;
			image_drawpixel(image, px, py, r, g, b);
		}
	}
	else /* line is more vertical than horizontal */
	{
		int x = dyabs >> 1;

		for (i = 0; i < dyabs; i++)
		{
			x += dxabs;
			if (x >= dyabs)
			{
				x -= dyabs;
				px += sdx;
			}
			py += sdy;
			image_drawpixel(image, px, py, r, g, b);
		}
	}
}
