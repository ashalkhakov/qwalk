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

#include <string.h>

#include "global.h"
#include "image.h"

typedef struct pcx_header_s
{
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	short xmin;
	short ymin;
	short xmax;
	short ymax;
	short hres;
	short vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	short bytes_per_line;
	short palette_type;
	char filler[58];
} pcx_header_t;

static void swap_pcx_header(pcx_header_t *header)
{
	header->xmin = LittleShort(header->xmin);
	header->ymin = LittleShort(header->ymin);
	header->xmax = LittleShort(header->xmax);
	header->ymax = LittleShort(header->ymax);
	header->hres = LittleShort(header->hres);
	header->vres = LittleShort(header->vres);
	header->bytes_per_line = LittleShort(header->bytes_per_line);
	header->palette_type = LittleShort(header->palette_type);
}

image_rgba_t *image_pcx_load(void *filedata, size_t filesize, char **out_error)
{
	image_rgba_t *image;
	unsigned char *f = (unsigned char*)filedata;
	/*unsigned char *endf = f + filesize;*/
	pcx_header_t *header;
	unsigned char *palette768;
	unsigned char *pix;
	int x, y;
	unsigned char databyte;
	int runlen;

	if (filesize < 128 + 1 + 768)
		return NULL;

	header = (pcx_header_t*)f;
	f += 128;

	swap_pcx_header(header);

	if (header->manufacturer != 0x0a)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad manufacturer");
		return NULL;
	}

	if (header->version != 5)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad version");
		return NULL;
	}

	if (header->encoding != 1)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad encoding");
		return NULL;
	}

	if (header->bits_per_pixel != 8)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad bits_per_pixel");
		return NULL;
	}

	if (header->xmax - header->xmin + 1 < 1 || header->xmax - header->xmin + 1 > 4096)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad xmax");
		return NULL;
	}

	if (header->ymax - header->ymin + 1 < 1 || header->ymax - header->ymin + 1 > 4096)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad ymax");
		return NULL;
	}

	if (header->color_planes != 1)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad color_planes");
		return NULL;
	}

	/* header.palette_type should be 1, but some pcxes use 0 or 2 for some reason, so ignore it */

/* grab the palette from the end of the file */
	palette768 = (unsigned char*)filedata + filesize - 768;

/* there is supposed to be an extra byte, 0x0c, before the palette. but, qME forgets this when exporting pcxes, so we'll
 * have to let it slide */
/*	if (palette768[-1] != 0x0c)
	{
		if (out_error)
			*out_error = msprintf("pcx: bad palette format");
		return NULL;
	}*/

	image = image_alloc(header->xmax - header->xmin + 1, header->ymax - header->ymin + 1);
	if (!image)
	{
		if (out_error)
			*out_error = msprintf("pcx: out of memory");
		return NULL;
	}

	for (y = 0; y < header->ymax - header->ymin + 1; y++)
	{
		pix = image->pixels + y * image->width * 4;

		for (x = 0; x < header->bytes_per_line; )
		{
			databyte = *f++;

			if ((databyte & 0xc0) == 0xc0)
			{
				runlen = databyte & 0x3f;
				databyte = *f++;
			}
			else
				runlen = 1;

			while (runlen-- > 0)
			{
				if (x < image->width)
				{
					pix[0] = palette768[databyte*3+0];
					pix[1] = palette768[databyte*3+1];
					pix[2] = palette768[databyte*3+2];
					pix[3] = 0xff;
					pix += 4;
				}
				x++;
			}
		}
	}

	return image;
}

size_t image_pcx_save(const image_paletted_t *image, void *filedata, size_t filesize)
{
	unsigned char *f;
	pcx_header_t *header;
	int x, y;

	f = (unsigned char*)filedata;

/* write header */
	header = (pcx_header_t*)f;
	f += 128;

	header->manufacturer = 0x0a;
	header->version = 5;
	header->encoding = 1;
	header->bits_per_pixel = 8;
	header->xmin = 0;
	header->ymin = 0;
	header->xmax = image->width - 1;
	header->ymax = image->height - 1;
	header->hres = 72; /* hres and vres are dots-per-inch values, unused */
	header->vres = 72;
	memcpy(header->palette, image->palette.rgb, 48); /* not useful */
	header->reserved = 0;
	header->color_planes = 1;
	header->bytes_per_line = (image->width + 1) & ~1; /* round up to multiple of 2 */
	header->palette_type = 1; /* according to pcx specs the accepted values are 1 (colour/bw) and 2 (greyscale) */
	memset(header->filler, 0, 58);

	swap_pcx_header(header);

/* write image */
	for (y = 0; y < image->height; y++)
	{
		const unsigned char *pix = image->pixels + y * image->width;

		for (x = 0; x < header->bytes_per_line; )
		{
			unsigned char pix_x = (x < image->width) ? pix[x] : pix[image->width - 1];
			int runlen;

			for (runlen = 1; runlen < 63 && x + runlen < header->bytes_per_line; runlen++)
				if (pix_x != (((x + runlen) < image->width) ? pix[x + runlen] : pix[image->width - 1]))
					break;

			if (runlen == 1)
			{
				if ((pix_x & 0xc0) == 0xc0)
					*f++ = 0xc1;
				*f++ = pix_x;
			}
			else
			{
				*f++ = 0xc0 | runlen;
				*f++ = pix_x;
			}

			x += runlen;
		}
	}

/* write palette */
	*f++ = 0x0c;
	memcpy(f, image->palette.rgb, 768);
	f += 768;

	return f - (unsigned char*)filedata;
}
