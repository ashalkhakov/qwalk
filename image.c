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

/* FIXME - rewrite the minification code... it's ugly, probably slow, and maybe also has some weighting issues */
image_rgba_t *image_resize(const image_rgba_t *source, int newwidth, int newheight)
{
	image_rgba_t *intermediate;
	image_rgba_t *image;
	int x, y, i;

	intermediate = image_alloc(newwidth, source->height);
	if (!intermediate)
		return NULL;

	image = image_alloc(newwidth, newheight);
	if (!image)
	{
		image_free(&intermediate);
		return NULL;
	}

/* horizontal resize */
	if (newwidth > source->width)
	{
		for (x = 0; x < newwidth; x++)
		{
			float fx = (float)x * (float)source->width / (float)newwidth;
			int x1 = (int)fx;
			int x2 = min(x1 + 1, source->width - 1);
			float fx2 = fx - (float)x1;
			float fx1 = 1.0f - fx2;
			int ifx1 = (int)(fx1 * 256.0f);
			int ifx2 = (int)(fx2 * 256.0f);
			unsigned char *out = intermediate->pixels + x * 4;
			const unsigned char *in1 = source->pixels + x1 * 4;
			const unsigned char *in2 = source->pixels + x2 * 4;

			for (y = 0; y < source->height; y++, out += newwidth * 4, in1 += source->width * 4, in2 += source->width * 4)
				for (i = 0; i < 4; i++)
					out[i] = (in1[i] * ifx1 + in2[i] * ifx2 + 127) >> 8;
		}
	}
	else if (newwidth < source->width)
	{
		for (y = 0; y < source->height; y++)
		for (x = 0; x < newwidth; x++)
		{
			const unsigned char *in = source->pixels + y * source->width * 4;
			float colour[4], count;
			float fxx = (x * source->width) / (float)newwidth;
			int x1 = ((x - 1) * source->width + newwidth / 2) / newwidth;
			int x2 = ((x + 1) * source->width + newwidth / 2) / newwidth;

			x1 = max(x1, 0);
			x2 = min(x2, source->width - 1);

			colour[0] = colour[1] = colour[2] = colour[3] = 0;
			count = 0;

			for (i = x1; i <= x2; i++)
			{
				float dist;

				if (i < fxx)
					dist = (fxx - x1) - (fxx - i);
				else if (i > fxx)
					dist = (x2 - fxx) - (i - fxx);
				else
					dist = max(x2 - fxx, fxx - x1);

				dist *= dist; /* square it to increase sharpness a little */

				colour[0] += dist * in[i*4+0];
				colour[1] += dist * in[i*4+1];
				colour[2] += dist * in[i*4+2];
				colour[3] += dist * in[i*4+3];
				count += dist;
			}

			if (!count)
				count = 1; /* i don't THINK this should happen... */

			for (i = 0; i < 4; i++)
				intermediate->pixels[(y*newwidth+x)*4+i] = (unsigned char)(colour[i] / count);
		}
	}
	else
	{
		memcpy(intermediate->pixels, source->pixels, source->width * source->height * 4);
	}

/* vertical resize */
	if (newheight > source->height)
	{
		for (y = 0; y < newheight; y++)
		{
			float fy = (float)y * (float)source->height / (float)newheight;
			int y1 = (int)fy;
			int y2 = min(y1 + 1, source->height - 1);
			float fy2 = fy - (float)y1;
			float fy1 = 1.0f - fy2;
			int ify1 = (int)(fy1 * 256.0f);
			int ify2 = (int)(fy2 * 256.0f);
			unsigned char *out = image->pixels + y * newwidth * 4;
			const unsigned char *in1 = intermediate->pixels + y1 * newwidth * 4;
			const unsigned char *in2 = intermediate->pixels + y2 * newwidth * 4;

			for (x = 0; x < newwidth; x++, out += 4, in1 += 4, in2 += 4)
				for (i = 0; i < 4; i++)
					out[i] = (in1[i] * ify1 + in2[i] * ify2 + 127) >> 8;
		}
	}
	else if (newheight < source->height)
	{
		for (y = 0; y < newheight; y++)
		for (x = 0; x < newwidth; x++)
		{
			float colour[4], count;
			float fyy = (y * source->height) / (float)newheight;
			int y1 = ((y - 1) * source->height + newheight / 2) / newheight;
			int y2 = ((y + 1) * source->height + newheight / 2) / newheight;

			y1 = max(y1, 0);
			y2 = min(y2, source->height - 1);

			colour[0] = colour[1] = colour[2] = colour[3] = 0;
			count = 0;

			for (i = y1; i <= y2; i++)
			{
				const unsigned char *in = intermediate->pixels + (i * newwidth + x) * 4;
				float dist;

				if (i < fyy)
					dist = (fyy - y1) - (fyy - i);
				else if (i > fyy)
					dist = (y2 - fyy) - (i - fyy);
				else
					dist = max(y2 - fyy, fyy - y1);

				dist *= dist; /* square it to increase sharpness a little */

				colour[0] += dist * in[0];
				colour[1] += dist * in[1];
				colour[2] += dist * in[2];
				colour[3] += dist * in[3];
				count += dist;
			}

			if (!count)
				count = 1; /* i don't THINK this should happen... */

			for (i = 0; i < 4; i++)
				image->pixels[(y*newwidth+x)*4+i] = (unsigned char)(colour[i] / count);
		}
	}
	else
	{
		memcpy(image->pixels, intermediate->pixels, newwidth * newheight * 4);
	}

	image_free(&intermediate);

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
