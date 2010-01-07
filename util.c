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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
# include <windows.h>
#else
# include <unistd.h>
# include <fcntl.h>
# include <dlfcn.h>
# include <errno.h>
#endif

#include "global.h"

unsigned short SwapShort(unsigned short v)
{
	unsigned char b1 = v & 0xFF;
	unsigned char b2 = (v >> 8) & 0xFF;

	return ((unsigned short)b1 << 8) + (unsigned short)b2;
}

unsigned int SwapLong(unsigned int v)
{
	unsigned char b1 = v & 0xFF;
	unsigned char b2 = (v >> 8) & 0xFF;
	unsigned char b3 = (v >> 16) & 0xFF;
	unsigned char b4 = (v >> 24) & 0xFF;

	return ((unsigned int)b1 << 24) + ((unsigned int)b2 << 16) + ((unsigned int)b3 << 8) + (unsigned int)b4;
}

float SwapFloat(float v)
{
	union
	{
		float f;
		unsigned char b[4];
	} dat1, dat2;

	dat1.f = v;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

/* FIXME - do this properly */
char *msprintf(const char *format, ...)
{
	va_list ap;
	static char buffer[16384];
	char *str;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	str = (char*)qmalloc(strlen(buffer) + 1);
	if (str == NULL)
		return NULL;
	strcpy(str, buffer);
	return str;
}

void strlcpy(char *dest, const char *src, size_t size)
{
	size_t i;

	memset(dest, 0, size);

	for (i = 0; i < size - 1; i++)
	{
		dest[i] = src[i];

		if (src[i] == '\0')
			break;
	}
}

bool_t loadfile(const char *filename, void **out_data, size_t *out_size, char **out_error)
{
	FILE *fp;
	long ftellret;
	unsigned char *filemem;
	size_t filesize, readsize;

	*out_data = NULL;
	if (out_size)
		*out_size = 0;
	if (out_error)
		*out_error = NULL;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		*out_error = msprintf("Couldn't open file: %s", strerror(errno));
		return false;
	}

	fseek(fp, 0, SEEK_END);
	ftellret = ftell(fp);
	if (ftellret < 0) /* ftell returns -1L and sets errno on failure */
	{
		*out_error = msprintf("Couldn't seek to end of file: %s", strerror(errno));
		fclose(fp);
		return false;
	}
	filesize = (size_t)ftellret;
	fseek(fp, 0, SEEK_SET);

	filemem = (unsigned char*)qmalloc(filesize + 1);
	if (!filemem)
	{
		*out_error = msprintf("Couldn't allocate memory to open file: %s", strerror(errno));
		fclose(fp);
		return false;
	}

	readsize = fread(filemem, 1, filesize, fp);
	fclose(fp);

	if (readsize < filesize)
	{
		*out_error = msprintf("Failed to read file: %s", strerror(errno));
		qfree(filemem);
		return false;
	}

	filemem[filesize] = 0;

	*out_data = (void*)filemem;
	if (out_size)
		*out_size = filesize;

	return true;
}

bool_t writefile(const char *filename, const void *data, size_t size, char **out_error)
{
	FILE *fp;
	size_t wrotesize;

	fp = fopen(filename, "wb");
	if (!fp)
	{
		*out_error = msprintf("Couldn't open file: %s", strerror(errno));
		return false;
	}

	wrotesize = fwrite(data, 1, size, fp);
	fclose(fp);

	if (wrotesize < size)
	{
		*out_error = msprintf("Failed to write file: %s", strerror(errno));
		return false;
	}

	return true;
}

/* memory management wrappers (leak detection) */

/*#define SENTINEL 0xDEADBEEF*/

typedef struct alloc_s
{
	unsigned int sentinel;

	size_t numbytes;
	const char *file;
	int line;

	struct alloc_s *prev, *next;
} alloc_t;

static alloc_t *alloc_head = NULL, *alloc_tail = NULL;

void *qmalloc_(size_t numbytes, const char *file, int line)
{
	alloc_t *alloc;
	void *mem;

/*	numbytes = (numbytes + 7) & ~7;*/

	mem = malloc(sizeof(alloc_t) + numbytes);

	if (!mem)
		return NULL;

	alloc = (alloc_t*)mem;
	alloc->numbytes = numbytes;
	alloc->file = file;
	alloc->line = line;
	alloc->prev = alloc_tail;
	alloc->next = NULL;
	if (!alloc_head)
		alloc_head = alloc;
	else
		alloc_tail->next = alloc;
	alloc_tail = alloc;

	return alloc + 1;
}

void qfree(void *mem)
{
	alloc_t *alloc;

	if (!mem)
		return;

	alloc = (alloc_t*)mem - 1;

	if (alloc->prev) alloc->prev->next = alloc->next;
	if (alloc->next) alloc->next->prev = alloc->prev;
	if (alloc_head == alloc) alloc_head = alloc->next;
	if (alloc_tail == alloc) alloc_tail = alloc->prev;

	free(alloc);
}

void dumpleaks(void)
{
	alloc_t *alloc, *next;

	if (!alloc_head)
		return;

	printf("Memory leak(s):\n");
	for (alloc = alloc_head; alloc; alloc = next)
	{
		printf("%s (ln %d) (%d bytes)\n", alloc->file, alloc->line, alloc->numbytes);
		next = alloc->next;
		free(alloc);
	}

	alloc_head = NULL;
	alloc_tail = NULL;
}

/* dll management (taken from darkplaces) */

bool_t loadlibrary(const char *dllname, dllhandle_t *handle, const dllfunction_t *functions)
{
	const dllfunction_t *func;
	dllhandle_t dllhandle = 0;

	if (!handle)
		return false;

	for (func = functions; func && func->name; func++)
		*func->funcvariable = NULL;

	printf("Trying to load library \"%s\"... ", dllname);

#ifdef WIN32
	dllhandle = LoadLibraryA(dllname); /* FIXME - change project settings to use ASCII windows functions? */
#else
	dllhandle = dlopen(dllname, RTLD_LAZY | RTLD_GLOBAL);
#endif

	if (!dllhandle)
	{
		printf("failed.\n");
		return false;
	}

	printf("loaded.\n");

	for (func = functions; func && func->name; func++)
	{
		*func->funcvariable = getprocaddress(dllhandle, func->name);
		if (!*func->funcvariable)
		{
			printf("Missing function \"%s\" - broken library!\n", func->name);
			unloadlibrary(&dllhandle);
			return false;
		}
	}

	*handle = dllhandle;
	return true;
}

void unloadlibrary(dllhandle_t *handle)
{
	if (!handle || !*handle)
		return;

#ifdef WIN32
	FreeLibrary(*handle);
#else
	dlclose(*handle);
#endif

	*handle = NULL;
}

void *getprocaddress(dllhandle_t handle, const char *name)
{
#ifdef WIN32
	return (void*)GetProcAddress(handle, name);
#else
	return (void*)dlsym(handle, name);
#endif
}
