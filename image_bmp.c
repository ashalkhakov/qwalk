/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2023-2024 Artyom Shalkhakov

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <string.h>

#include "global.h"
#include "image.h"

typedef struct
{
	char id[2];
	unsigned long fileSize;
	unsigned long reserved0;
	unsigned long bitmapDataOffset;
	unsigned long bitmapHeaderSize;
	unsigned long width;
	unsigned long height;
	unsigned short planes;
	unsigned short bitsPerPixel;
	unsigned long compression;
	unsigned long bitmapDataSize;
	unsigned long hRes;
	unsigned long vRes;
	unsigned long colors;
	unsigned long importantColors;
	unsigned char palette[256][4];
} BMPHeader_t;

image_rgba_t *image_bmp_load(mem_pool_t *pool, void *filedata, size_t filesize, char **out_error)
{
	int		columns, rows, numPixels;
	unsigned char	*pixbuf;
	int		row, column;
	unsigned char	*buf_p;
	unsigned char	*buffer;
	int		length = filesize;
	BMPHeader_t bmpHeader;
	unsigned char		*bmpRGBA;
	image_rgba_t *image;

	buf_p = filedata;

	bmpHeader.id[0] = *buf_p++;
	bmpHeader.id[1] = *buf_p++;
	bmpHeader.fileSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.reserved0 = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapDataOffset = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapHeaderSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.width = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.height = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.planes = LittleShort( * ( short * ) buf_p );
	buf_p += 2;
	bmpHeader.bitsPerPixel = LittleShort( * ( short * ) buf_p );
	buf_p += 2;
	bmpHeader.compression = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapDataSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.hRes = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.vRes = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.colors = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.importantColors = LittleLong( * ( long * ) buf_p );
	buf_p += 4;

	memcpy( bmpHeader.palette, buf_p, sizeof( bmpHeader.palette ) );

	if ( bmpHeader.bitsPerPixel == 8 )
		buf_p += 1024;

	if ( bmpHeader.id[0] != 'B' && bmpHeader.id[1] != 'M' ) 
	{
		if (out_error)
			*out_error = msprintf("only Windows-style BMP files supported\n");
        return NULL;
	}
	if ( bmpHeader.fileSize != length )
	{
		if (out_error)
			*out_error = msprintf("header size does not match file size (%d vs. %d)\n", bmpHeader.fileSize, length);
        return NULL;
	}
	if ( bmpHeader.compression != 0 )
	{
		if (out_error)
			*out_error = msprintf("only uncompressed BMP files supported\n");
        return NULL;
	}
	if ( bmpHeader.bitsPerPixel < 8 )
	{
		if (out_error)
			*out_error = msprintf("monochrome and 4-bit BMP files not supported\n");
        return NULL;
	}

	columns = bmpHeader.width;
	rows = bmpHeader.height;
	if ( rows < 0 )
		rows = -rows;
	numPixels = columns * rows;

    image = image_alloc(pool, columns, rows);
    if (!image)
    {
        if (out_error)
            *out_error = msprintf("bmp: out of memory");
        return NULL;
    }
	bmpRGBA = image->pixels;


	for ( row = rows-1; row >= 0; row-- )
	{
		pixbuf = bmpRGBA + row*columns*4;

		for ( column = 0; column < columns; column++ )
		{
			unsigned char red, green, blue, alpha;
			int palIndex;
			unsigned short shortPixel;

			switch ( bmpHeader.bitsPerPixel )
			{
			case 8:
				palIndex = *buf_p++;
				*pixbuf++ = bmpHeader.palette[palIndex][2];
				*pixbuf++ = bmpHeader.palette[palIndex][1];
				*pixbuf++ = bmpHeader.palette[palIndex][0];
				*pixbuf++ = 0xff;
				break;
			case 16:
				shortPixel = * ( unsigned short * ) pixbuf;
				pixbuf += 2;
				*pixbuf++ = ( shortPixel & ( 31 << 10 ) ) >> 7;
				*pixbuf++ = ( shortPixel & ( 31 << 5 ) ) >> 2;
				*pixbuf++ = ( shortPixel & ( 31 ) ) << 3;
				*pixbuf++ = 0xff;
				break;

			case 24:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = 255;
				break;
			case 32:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				alpha = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				break;
			default:
                if (out_error)
                    *out_error = msprintf("illegal pixel_size '%d'\n", bmpHeader.bitsPerPixel);
                
                return NULL;
			}
		}
	}

	return image;
}
