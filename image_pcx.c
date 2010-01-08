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

bool_t image_pcx_load(void *filedata, size_t filesize, image_rgba_t *out_image)
{
	unsigned char *f = (unsigned char*)filedata;
	unsigned char *endf = f + filesize;
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin;
	unsigned short ymin;
	unsigned short xmax;
	unsigned short ymax;
	unsigned short hres;
	unsigned short vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
	unsigned char *palette768;
	unsigned char *outpixels, *pix;
	int x, y;
	unsigned char databyte;
	int runlen;

	if (filesize < 128)
		return false;

	manufacturer   = f[0];
	version        = f[1];
	encoding       = f[2];
	bits_per_pixel = f[3];
	xmin           = f[4] + f[5] * 256;
	ymin           = f[6] + f[7] * 256;
	xmax           = f[8] + f[9] * 256;
	ymax           = f[10] + f[11] * 256;
	hres           = f[12] + f[13] * 256;
	vres           = f[14] + f[15] * 256;
	memcpy(palette, f + 16, 48);
	reserved       = f[64];
	color_planes   = f[65];
	bytes_per_line = f[66] + f[67] * 256;
	palette_type   = f[68] + f[69] * 256;
	memcpy(filler, f + 70, 58);

	f += 128;

	if (manufacturer != 0x0a)
	{
		printf("pcx: bad manufacturer\n");
		return false;
	}

	if (version != 5)
	{
		printf("pcx: bad version\n");
		return false;
	}

	if (encoding != 1)
	{
		printf("pcx: bad encoding\n");
		return false;
	}

	if (bits_per_pixel != 8)
	{
		printf("pcx: bad bits_per_pixel\n");
		return false;
	}

	if (xmax > 320) /* width = xmax - xmin + 1 */
	{
		printf("pcx: bad xmax\n");
		return false;
	}

	if (ymax > 256) /* height = ymax - ymin + 1 */
	{
		printf("pcx: bad ymax\n");
		return false;
	}

/* grab the palette from the end of the file */
	palette768 = (unsigned char*)filedata + filesize - 768;
	if (palette768[-1] != 0x0c)
	{
		printf("pcx: bad palette format\n");
		return false;
	}

	outpixels = (unsigned char*)qmalloc((xmax + 1) * (ymax + 1) * 4);
	if (!outpixels)
	{
		printf("pcx: out of memory\n");
		return false;
	}

	for (y = 0; y <= ymax; y++)
	{
		pix = outpixels + 4 * y * (xmax + 1);

		for (x = 0; x <= xmax; )
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
				pix[0] = palette768[databyte*3+0];
				pix[1] = palette768[databyte*3+1];
				pix[2] = palette768[databyte*3+2];
				pix[3] = 0xff;
				pix += 4;
				x++;
			}
		}
	}

	out_image->pixels = outpixels;
	out_image->width = xmax + 1;
	out_image->height = ymax + 1;
	return true;
}
