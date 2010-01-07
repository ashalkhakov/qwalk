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

/* TGA loading code hastily ripped from DarkPlaces */
/* TODO - rewrite it to be shorter, speed isn't as important when we're not loading hundreds of them */

#include <stdio.h>

#include "global.h"
#include "image.h"

bool_t image_tga_load(void *filedata, size_t filesize, image_rgba_t *out_image)
{
	unsigned char *f = (unsigned char*)filedata;
	unsigned char *endf = f + filesize;
	int id_length;
	int colormap_type;
	int image_type;
	int colormap_index;
	int colormap_length;
	int colormap_size;
	int x_origin;
	int y_origin;
	int width;
	int height;
	int pixel_size;
	int attributes;
	bool_t bottom_to_top;
	bool_t compressed;
	int x, y, readpixelcount, red, green, blue, alpha, runlen, row_inc;
	unsigned char palette[256*4], *p;
	unsigned char *outpixels, *pixbuf;

	if (filesize < 19)
		return false;

/* load header manually because not all of these values are aligned */
	id_length       = f[0];
	colormap_type   = f[1];
	image_type      = f[2];
	colormap_index  = f[3] + f[4] * 256;
	colormap_length = f[5] + f[6] * 256;
	colormap_size   = f[7];
	x_origin        = f[8] + f[9] * 256;
	y_origin        = f[10] + f[11] * 256;
	width           = f[12] + f[13] * 256;
	height          = f[14] + f[15] * 256;
	pixel_size      = f[16];
	attributes      = f[17];

	f += 18;

	f += id_length; /* skip comment */

	if (width > 4096 || height > 4096 || width <= 0 || height <= 0)
	{
		printf("tga: bad dimensions\n");
		return false;
	}

	if (attributes & 0x10) /* this bit indicates origin on the right, which we don't support */
	{
		printf("tga: bad origin (must be top left or bottom left)\n");
		return false;
	}

/* if bit 5 of attributes isn't set, the image has been stored from bottom to top */
	bottom_to_top = (attributes & 0x20) == 0;

	compressed = false;
	x = 0;
	y = 0;
	red = 255;
	green = 255;
	blue = 255;
	alpha = 255;
	switch (image_type)
	{
	default:
		printf("tga: bad image type\n");
		return false;

	/* colormapped */
	case 9: compressed = true;
	case 1:
		if (pixel_size != 8)
		{
			printf("tga: bad pixel_size\n");
			return false;
		}
		if (colormap_length != 256)
		{
			printf("tga: bad colormap_length\n");
			return false;
		}
		if (colormap_index != 0)
		{
			printf("tga: bad colormap_index\n");
			return false;
		}

		switch (colormap_size)
		{
		default:
			printf("tga: bad colormap_size\n");
			return false;
		case 24:
			for (x = 0; x < colormap_length; x++)
			{
				palette[x*4+2] = *f++;
				palette[x*4+1] = *f++;
				palette[x*4+0] = *f++;
				palette[x*4+3] = 255;
			}
			break;
		case 32:
			for (x = 0; x < colormap_length; x++)
			{
				palette[x*4+2] = *f++;
				palette[x*4+1] = *f++;
				palette[x*4+0] = *f++;
				palette[x*4+3] = *f++;
			}
			break;
		}

		outpixels = (unsigned char*)qmalloc(width * height * 4);
		if (!outpixels)
		{
			printf("tga: out of memory\n");
			return false;
		}

		if (bottom_to_top)
		{
			pixbuf = outpixels + (height - 1) * width * 4;
			row_inc = -width * 4 * 2;
		}
		else
		{
			pixbuf = outpixels;
			row_inc = 0;
		}

		if (compressed)
		{
			while (y < height)
			{
				readpixelcount = 1000000;
				runlen = 1000000;

				if (f < endf)
				{
					runlen = *f++;
				/* high bit indicates this is an RLE compressed run */
					if (runlen & 0x80)
						readpixelcount = 1;
					runlen = 1 + (runlen & 0x7f);
				}

				while ((runlen--) && y < height)
				{
					if (readpixelcount > 0)
					{
						readpixelcount--;

						red = 255;
						green = 255;
						blue = 255;
						alpha = 255;

						if (f < endf)
						{
							p = palette + (*f++) * 4;
							red = p[0];
							green = p[1];
							blue = p[2];
							alpha = p[3];
						}
					}

					pixbuf[0] = red;
					pixbuf[1] = green;
					pixbuf[2] = blue;
					pixbuf[3] = alpha;
					pixbuf += 4;

					if (++x == width)
					{
					/* end of line, advance to next */
						x = 0;
						y++;
						pixbuf += row_inc;
					}
				}
			}
		}
		else
		{
			while (y < height)
			{
				red = 255;
				green = 255;
				blue = 255;
				alpha = 255;

				if (f < endf)
				{
					p = palette + (*f++) * 4;
					red = p[0];
					green = p[1];
					blue = p[2];
					alpha = p[3];
				}

				pixbuf[0] = red;
				pixbuf[1] = green;
				pixbuf[2] = blue;
				pixbuf[3] = alpha;
				pixbuf += 4;

				if (++x == width)
				{
				/* end of line, advance to next */
					x = 0;
					y++;
					pixbuf += row_inc;
				}
			}
		}
		break;

	/* BGR or BGRA */
	case 10: compressed = true;
	case 2:
		if (pixel_size != 24 && pixel_size != 32)
		{
			printf("tga: bad pixel_size\n");
			return false;
		}

		outpixels = (unsigned char*)qmalloc(width * height * 4);
		if (!outpixels)
		{
			printf("tga: out of memory\n");
			return false;
		}

		if (bottom_to_top)
		{
			pixbuf = outpixels + (height - 1) * width * 4;
			row_inc = -width * 4 * 2;
		}
		else
		{
			pixbuf = outpixels;
			row_inc = 0;
		}

		if (compressed)
		{
			while (y < height)
			{
				readpixelcount = 1000000;
				runlen = 1000000;

				if (f < endf)
				{
					runlen = *f++;
				/* high bit indicates this is an RLE compressed run */
					if (runlen & 0x80)
						readpixelcount = 1;
					runlen = 1 + (runlen & 0x7f);
				}

				while ((runlen--) && y < height)
				{
					if (readpixelcount > 0)
					{
						readpixelcount--;

						blue  = (f < endf) ? *f++ : 255;
						green = (f < endf) ? *f++ : 255;
						red   = (f < endf) ? *f++ : 255;
						alpha = (f < endf && pixel_size == 32) ? *f++ : 255;
					}

					pixbuf[0] = red;
					pixbuf[1] = green;
					pixbuf[2] = blue;
					pixbuf[3] = alpha;
					pixbuf += 4;

					if (++x == width)
					{
					/* end of line, advance to next */
						x = 0;
						y++;
						pixbuf += row_inc;
					}
				}
			}
		}
		else
		{
			while (y < height)
			{
				pixbuf[2] = (f < endf) ? *f++ : 255;
				pixbuf[1] = (f < endf) ? *f++ : 255;
				pixbuf[0] = (f < endf) ? *f++ : 255;
				pixbuf[3] = (f < endf && pixel_size == 32) ? *f++ : 255;
				pixbuf += 4;

				if (++x == width)
				{
				/* end of line, advance to next */
					x = 0;
					y++;
					pixbuf += row_inc;
				}
			}
		}
		break;

	/* greyscale */
	case 11: compressed = true;
	case 3:
		if (pixel_size != 8)
		{
			printf("tga: bad pixel_size\n");
			return false;
		}

		outpixels = (unsigned char*)qmalloc(width * height * 4);
		if (!outpixels)
		{
			printf("tga: out of memory\n");
			return false;
		}

		if (bottom_to_top)
		{
			pixbuf = outpixels + (height - 1) * width * 4;
			row_inc = -width * 4 * 2;
		}
		else
		{
			pixbuf = outpixels;
			row_inc = 0;
		}

		if (compressed)
		{
			while (y < height)
			{
				readpixelcount = 1000000;
				runlen = 1000000;

				if (f < endf)
				{
					runlen = *f++;
				/* high bit indicates this is an RLE compressed run */
					if (runlen & 0x80)
						readpixelcount = 1;
					runlen = 1 + (runlen & 0x7f);
				}

				while ((runlen--) && y < height)
				{
					if (readpixelcount > 0)
					{
						readpixelcount--;

						red = (f < endf) ? *f++ : 255;
					}

					*pixbuf++ = red;

					if (++x == width)
					{
					/* end of line, advance to next */
						x = 0;
						y++;
						pixbuf += row_inc;
					}
				}
			}
		}
		else
		{
			while (y < height)
			{
				*pixbuf++ = (f < endf) ? *f++ : 255;

				if (++x == width)
				{
				/* end of line, advance to next */
					x = 0;
					y++;
					pixbuf += row_inc;
				}
			}
		}
		break;
	}

	out_image->width = width;
	out_image->height = height;
	out_image->pixels = outpixels;
	return true;
}
