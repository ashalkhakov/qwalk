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

image_rgba_t *image_load(const char *filename, void *filedata, size_t filesize, char **out_error)
{
	const char *ext = strrchr(filename, '.');

	if (!ext)
	{
		if (out_error)
			*out_error = msprintf("%s: no file extension", filename);
		return NULL;
	}

	if (!strcasecmp(ext, ".pcx"))
		return image_pcx_load(filedata, filesize, out_error);
	if (!strcasecmp(ext, ".tga"))
		return image_tga_load(filedata, filesize, out_error);
	if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg"))
		return image_jpeg_load(filedata, filesize, out_error);

	if (out_error)
		*out_error = msprintf("unrecognized file extension \"%s\"", ext);
	return NULL;
}

image_rgba_t *image_alloc(int width, int height)
{
	image_rgba_t *image;
	if (width < 1 || height < 1)
		return NULL;
	image = (image_rgba_t*)qmalloc(sizeof(image_rgba_t) + width * height * 4);
	if (!image)
		return NULL;
	image->width = width;
	image->height = height;
	image->pixels = (unsigned char*)(image + 1);
	return image;
}

void image_free(image_rgba_t **image)
{
	qfree(*image);
	*image = NULL;
}

image_rgba_t *image_createfill(int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	image_rgba_t *image;
	int i;

	image = image_alloc(width, height);
	if (!image)
		return NULL;

	for (i = 0; i < width * height; i++)
	{
		image->pixels[i*4+0] = r;
		image->pixels[i*4+1] = g;
		image->pixels[i*4+2] = b;
		image->pixels[i*4+3] = a;
	}

	return image;
}

image_rgba_t *image_clone(const image_rgba_t *source)
{
	image_rgba_t *image;

	if (!source)
		return NULL;

	image = image_alloc(source->width, source->height);
	if (!image)
		return NULL;

	memcpy(image->pixels, source->pixels, source->width * source->height * 4);

	return image;
}

static unsigned char palettize_colour(const palette_t *palette, bool_t fullbright, const unsigned char rgb[3])
{
	int i, dist;
	int besti = -1, bestdist;

	for (i = 0; i < 256; i++)
	{
		if (fullbright != !!(palette->fullbright_flags[i >> 5] & (1U << (i & 31))))
			continue;

		dist = abs(palette->rgb[i*3+0] - rgb[0]) + abs(palette->rgb[i*3+1] - rgb[1]) + abs(palette->rgb[i*3+2] - rgb[2]);

		if (besti == -1 || dist < bestdist)
		{
			besti = i;
			bestdist = dist;
		}
	}

	return (besti != -1) ? besti : 0;
}

image_paletted_t *image_palettize(const palette_t *palette, const image_rgba_t *source_diffuse, const image_rgba_t *source_fullbright)
{
	bool_t palette_has_fullbrights;
	image_paletted_t *pimage;
	int i;

	pimage = (image_paletted_t*)qmalloc(sizeof(image_paletted_t) + source_diffuse->width * source_diffuse->height);
	if (!pimage)
		return NULL;
	pimage->width = source_diffuse->width;
	pimage->height = source_diffuse->height;
	pimage->pixels = (unsigned char*)(pimage + 1);
	pimage->palette = *palette;

	palette_has_fullbrights = false;
	for (i = 0; i < 8; i++)
		if (palette->fullbright_flags[i])
			palette_has_fullbrights = true;

	if (source_diffuse && source_diffuse->pixels && source_fullbright && source_fullbright->pixels)
	{
		const unsigned char *in_diffuse = source_diffuse->pixels;
		const unsigned char *in_fullbright = source_fullbright->pixels;
		unsigned char *out = pimage->pixels;
		for (i = 0; i < pimage->width * pimage->height; i++, in_diffuse += 4, in_fullbright += 4, out++)
		{
			if (in_fullbright[0] || in_fullbright[1] || in_fullbright[2])
				*out = palettize_colour(palette, palette_has_fullbrights, in_fullbright);
			else
				*out = palettize_colour(palette, false, in_diffuse);
		}
	}
	else if (source_diffuse && source_diffuse->pixels)
	{
		const unsigned char *in_diffuse = source_diffuse->pixels;
		unsigned char *out = pimage->pixels;
		for (i = 0; i < pimage->width * pimage->height; i++, in_diffuse += 4, out++)
			*out = palettize_colour(palette, false, in_diffuse);
	}
	else if (source_fullbright && source_fullbright->pixels)
	{
		const unsigned char *in_fullbright = source_fullbright->pixels;
		unsigned char *out = pimage->pixels;
		for (i = 0; i < pimage->width * pimage->height; i++, in_fullbright += 4, out++)
			*out = palettize_colour(palette, palette_has_fullbrights, in_fullbright);
	}
	else
	{
		memcpy(pimage->pixels, 0, pimage->width * pimage->height);
	}

	return pimage;
}

/* FIXME - incomplete - will fail if you try to make an image bigger */
image_rgba_t *image_resize(const image_rgba_t *source, int width, int height)
{
	image_rgba_t *image;
	int x, y;

	image = image_alloc(width, height);
	if (!image)
		return NULL;

	for (y = 0; y < height; y++)
	{
		unsigned char *out = image->pixels + y * width * 4;
		int y1, y2, yy;

		yy = ((y * source->height + source->height / 2) / height);
		y1 = (((y - 1) * source->height + source->height / 2) / height); if (y1 < 0) y1 = 0;
		y2 = (((y + 1) * source->height + source->height / 2) / height); if (y1 > source->height - 1) y1 = source->height - 1;

		for (x = 0; x < width; x++)
		{
			int x1, x2, xx;
			int i, j;
			int colour[4];
			int num;

			xx = ((x * source->width + source->width / 2) / width);
			x1 = (((x - 1) * source->width + source->width / 2) / width); if (x1 < 0) x1 = 0;
			x2 = (((x + 1) * source->width + source->width / 2) / width); if (x1 > source->width - 1) x1 = source->width - 1;

			colour[0] = 0;
			colour[1] = 0;
			colour[2] = 0;
			colour[3] = 0;
			num = 0;

			for (j = y1; j <= y2; j++)
			{
				const unsigned char *in = source->pixels + j * source->width * 4;
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

	return image;
}

/* pad an image to a larger size. the edge pixels will be repeated instead of filled with black, to avoid any unwanted
 * bleeding if mipmapped and/or rendered with texture filtering. */
image_rgba_t *image_pad(const image_rgba_t *source, int width, int height)
{
	image_rgba_t *image;
	const unsigned char *inpixels;
	unsigned char *outp;
	int x, y;

	if (width < source->width || height < source->height)
		return NULL;

	image = image_alloc(width, height);
	if (!image)
		return NULL;

	inpixels = source->pixels;
	outp = image->pixels;

	for (y = 0; y < source->height; y++)
	{
		memcpy(outp, inpixels, source->width * 4);
		outp += source->width * 4;
		inpixels += source->width * 4;

		for (x = source->width; x < width; x++, outp += 4)
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

	return image;
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
