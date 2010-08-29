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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef WIN32
# include <windows.h>
# include <direct.h>
#else
# include <unistd.h>
# include <fcntl.h>
# include <dlfcn.h>
# include <errno.h>
#endif

#include "global.h"

bool_t g_force_yes = false; /* automatically choose "yes" for all confirms? */

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

char *copystring(const char *string)
{
	size_t size = strlen(string) + 1;
	char *s = (char*)qmalloc(size);
	if (s)
		memcpy(s, string, size);
	return s;
}

/* FIXME - do this properly */
char *msprintf(const char *format, ...)
{
	va_list ap;
	static char buffer[16384];

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	return copystring(buffer);
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

bool_t yesno(void)
{
	char buffer[16];

	if (g_force_yes)
	{
		printf("y\n");
		return true;
	}

	fgets(buffer, sizeof(buffer), stdin);

	return (buffer[0] == 'y' || buffer[0] == 'Y') && (!buffer[1] || buffer[1] == '\r' || buffer[1] == '\n');
}

/* sorry about this nightmare! */
/* modifies the input string to contain the properly formatted path */
bool_t makepath(char *path, char **out_error)
{
	char *s;
	char *dirname, *dn;
	char *newpath;
	struct stat st;

/* parse the path into directories */
	newpath = (char*)qmalloc(strlen(path) + 1);
	newpath[0] = '\0';

	dirname = (char*)qmalloc(strlen(path) + 1);
	dn = dirname;

#define FAIL(err) \
	{ \
		if (out_error) \
			*out_error = err; \
		qfree(dirname); \
		qfree(newpath); \
		return false; \
	}

	for (s = path; ; s++)
	{
		if (*s == '/' || *s == '\\' || !*s)
		{
			*dn = '\0';
			if (dirname[0])
			{
				if (newpath[0])
					strcat(newpath, "/");
				strcat(newpath, dirname);

			/* does this directory already exist? */
				if (stat(newpath, &st) != 0)
				{
					if (errno != ENOENT)
						FAIL(msprintf("%s: %s", newpath, strerror(errno)))

				/* file doesn't exist */
					printf("directory %s doesn't exist. Create it? [y/N] ", newpath);
					if (!yesno())
						FAIL(copystring("user aborted operation"))

				/* create the directory */
#ifdef WIN32
					if (_mkdir(newpath) != 0)
#else
					if (mkdir(newpath, 0777) != 0)
#endif
						FAIL(msprintf("%s: %s", newpath, strerror(errno)))
				}
				else
				{
				/* file exists. is it a file or a directory? */
					if ((st.st_mode & S_IFMT) != S_IFDIR)
						FAIL(msprintf("%s is not a directory", newpath))
				/* else, it's a directory which already exists, which is good */
				}
			}
			else if (!newpath[0] && *s)
				FAIL(msprintf("can't begin path with slash"))
			dn = dirname;

			if (!*s)
				break;
		}
		else
		{
			*dn++ = *s;
		}
	}

#undef FAIL

	qfree(dirname);
	strcpy(path, newpath);
	qfree(newpath);
	return true;
}

FILE *openfile_write(const char *filename, char **out_error)
{
	bool_t file_exists;
	FILE *fp;

	file_exists = false;
	if ((fp = fopen(filename, "rb")))
	{
		file_exists = true;
		fclose(fp);
	}

	if (file_exists)
	{
		printf("File %s already exists. Overwrite? [y/N] ", filename);
		if (!yesno())
			return (out_error && (*out_error = msprintf("user aborted operation"))), NULL;
	}

	fp = fopen(filename, "wb");
	if (!fp)
		return (out_error && (*out_error = msprintf("couldn't open file: %s", strerror(errno)))), NULL;

	return fp;
}

bool_t loadfile(const char *filename, void **out_data, size_t *out_size, char **out_error)
{
	FILE *fp;
	long ftellret;
	unsigned char *filemem;
	size_t filesize, readsize;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		if (out_error)
			*out_error = msprintf("Couldn't open file: %s", strerror(errno));
		return false;
	}

	fseek(fp, 0, SEEK_END);
	ftellret = ftell(fp);
	if (ftellret < 0) /* ftell returns -1L and sets errno on failure */
	{
		if (out_error)
			*out_error = msprintf("Couldn't seek to end of file: %s", strerror(errno));
		fclose(fp);
		return false;
	}
	filesize = (size_t)ftellret;
	fseek(fp, 0, SEEK_SET);

	filemem = (unsigned char*)qmalloc(filesize + 1);
	if (!filemem)
	{
		if (out_error)
			*out_error = msprintf("Couldn't allocate memory to open file: %s", strerror(errno));
		fclose(fp);
		return false;
	}

	readsize = fread(filemem, 1, filesize, fp);
	fclose(fp);

	if (readsize < filesize)
	{
		if (out_error)
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

	fp = openfile_write(filename, out_error);
	if (!fp)
		return false;

	if (fwrite(data, 1, size, fp) < size)
	{
		if (out_error)
			*out_error = msprintf("Failed to write file: %s", strerror(errno));
		fclose(fp);
		return false;
	}

	fclose(fp);
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
static size_t bytes_alloced = 0;
static size_t peak_bytes = 0;

void *qmalloc_(size_t numbytes, const char *file, int line)
{
	alloc_t *alloc;
	void *mem;

	if (!numbytes)
		return NULL;

/*	numbytes = (numbytes + 7) & ~7;*/

	mem = malloc(sizeof(alloc_t) + numbytes);

	if (!mem)
		return NULL;

	bytes_alloced += numbytes;
	peak_bytes = max(peak_bytes, bytes_alloced);

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

	bytes_alloced -= alloc->numbytes;

	if (alloc->prev) alloc->prev->next = alloc->next;
	if (alloc->next) alloc->next->prev = alloc->prev;
	if (alloc_head == alloc) alloc_head = alloc->next;
	if (alloc_tail == alloc) alloc_tail = alloc->prev;

	free(alloc);
}

void dumpleaks(void)
{
	alloc_t *alloc, *next;

#ifdef _DEBUG
	printf("Peak memory allocated: %d bytes\n", peak_bytes);
#endif

	if (!alloc_head)
		return;

#ifdef _DEBUG
	printf("Memory leak(s):\n");
#endif
	for (alloc = alloc_head; alloc; alloc = next)
	{
#ifdef _DEBUG
		printf("%s (ln %d) (%d bytes)\n", alloc->file, alloc->line, alloc->numbytes);
#endif
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

/* atexit events */

typedef struct atexit_event_s
{
	void (*function)(void);

	struct atexit_event_s *next;
} atexit_event_t;

static atexit_event_t *atexit_event_head = NULL;
static atexit_event_t *atexit_event_tail = NULL;

static void (*atexit_final_event)(void) = NULL;

void add_atexit_event(void (*function)(void))
{
	atexit_event_t *e = (atexit_event_t*)qmalloc(sizeof(atexit_event_t));

	e->function = function;
	e->next = NULL;

	if (!atexit_event_head)
		atexit_event_head = e;
	if (atexit_event_tail)
		atexit_event_tail->next = e;
	atexit_event_tail = e;
}

void set_atexit_final_event(void (*function)(void))
{
	atexit_final_event = function;
}

void call_atexit_events(void)
{
	atexit_event_t *e, *next;

	for (e = atexit_event_head; e; e = next)
	{
		next = e->next;
		(*e->function)();
		qfree(e);
	}

	atexit_event_head = NULL;
	atexit_event_tail = NULL;

	if (atexit_final_event)
	{
		(*atexit_final_event)();
		atexit_final_event = NULL;
	}
}

/* expandable buffers */

typedef struct xbuf_block_s
{
	unsigned char *memory;

	size_t bytes_written;

	struct xbuf_block_s *prev, *next;
} xbuf_block_t;

struct xbuf_s
{
	size_t block_size;

	xbuf_block_t *block_head, *block_tail;

	size_t bytes_written;

	FILE *fp;

	char *error;
};

/* if this is a file buffer, flush the block's contents into the file and reset the block */
static void xbuf_flush(xbuf_t *xbuf)
{
	if (xbuf->error || !xbuf->fp)
		return;

	if (fwrite(xbuf->block_head->memory, 1, xbuf->block_head->bytes_written, xbuf->fp) < xbuf->block_head->bytes_written)
	{
		xbuf->error = msprintf("failed to write to file: %s", strerror(errno));
		return;
	}

	xbuf->block_head->bytes_written = 0;
}

static xbuf_block_t *xbuf_new_block(xbuf_t *xbuf)
{
	xbuf_block_t *block;

	block = (xbuf_block_t*)qmalloc(sizeof(xbuf_block_t) + xbuf->block_size);
	if (!block)
		return NULL;

	block->memory = (unsigned char*)(block + 1);
	block->bytes_written = 0;

	block->prev = xbuf->block_tail;
	block->next = NULL;

	if (block->prev)
		block->prev->next = block;

	if (!xbuf->block_head)
		xbuf->block_head = block;
	xbuf->block_tail = block;

	return block;
}

xbuf_t *xbuf_create_memory(size_t block_size, char **out_error)
{
	xbuf_t *xbuf;

	if (block_size < 4096)
		block_size = 4096;

	xbuf = (xbuf_t*)qmalloc(sizeof(xbuf_t));
	if (!xbuf)
		return (out_error && (*out_error = msprintf("out of memory"))), NULL;

	xbuf->block_size = block_size;
	xbuf->block_head = NULL;
	xbuf->block_tail = NULL;
	xbuf->bytes_written = 0;
	xbuf->fp = NULL;
	xbuf->error = NULL;

	if (!xbuf_new_block(xbuf)) /* create the first block */
	{
		qfree(xbuf);
		return (out_error && (*out_error = msprintf("out of memory"))), NULL;
	}

	return xbuf;
}

xbuf_t *xbuf_create_file(size_t block_size, const char *filename, char **out_error)
{
	xbuf_t *xbuf;

	if (!filename)
		return (out_error && (*out_error = msprintf("no filename specified"))), NULL;

	xbuf = xbuf_create_memory(block_size, out_error);
	if (!xbuf)
		return NULL;

	xbuf->fp = openfile_write(filename, out_error);
	if (!xbuf->fp)
	{
		xbuf_free(xbuf, NULL);
		return NULL;
	}

	return xbuf;
}

bool_t xbuf_free(xbuf_t *xbuf, char **out_error)
{
	char *error = xbuf->error;
	xbuf_block_t *block, *nextblock;

	if (xbuf->fp)
		fclose(xbuf->fp);

	for (block = xbuf->block_head; block; block = nextblock)
	{
		nextblock = block->next;
		qfree(block);
	}

	qfree(xbuf);

	if (error)
	{
		if (out_error)
			*out_error = error;
		else
			qfree(error);
		return false;
	}
	else
	{
		return true;
	}
}

/* write a piece of data, which might end up spanning multiple blocks */
void xbuf_write_data(xbuf_t *xbuf, size_t length, const void *data)
{
	xbuf_block_t *block;

	if (xbuf->error)
		return;

	block = xbuf->block_tail;

	for (;;)
	{
		if (length < xbuf->block_size - block->bytes_written)
		{
		/* data fits in current block */
			memcpy(block->memory + block->bytes_written, data, length);

			xbuf->bytes_written += length;
			block->bytes_written += length;

			return;
		}
		else
		{
		/* fill the rest of the current block with as much data as fits */
			size_t amt = xbuf->block_size - block->bytes_written;

			memcpy(block->memory + block->bytes_written, data, amt);

			xbuf->bytes_written += amt;
			block->bytes_written += amt;

			length -= amt;
			data = (unsigned char*)data + amt;

		/* get more space */
			if (xbuf->fp)
				xbuf_flush(xbuf);
			else
				block = xbuf_new_block(xbuf);
		}
	}
}

void xbuf_write_byte(xbuf_t *xbuf, unsigned char byte)
{
	xbuf_write_data(xbuf, 1, &byte);
}

/* reserve a contiguous piece of memory */
void *xbuf_reserve_data(xbuf_t *xbuf, size_t length)
{
	xbuf_block_t *block;
	void *memory;

	if (xbuf->error)
		return NULL;
	if (xbuf->fp)
		return NULL; /* this won't work on file buffers currently (FIXME - is it possible?) */
	if (length > xbuf->block_size)
		return NULL;

/* get the current block */
	block = xbuf->block_tail;

/* if it won't fit in the current block, get more space */
	if (length >= xbuf->block_size - block->bytes_written)
	{
		if (xbuf->fp)
			xbuf_flush(xbuf);
		else
			block = xbuf_new_block(xbuf);
	}

/* return memory pointer and advance bytes_written */
	memory = block->memory + block->bytes_written;

	xbuf->bytes_written += length;
	block->bytes_written += length;

	return memory;
}

int xbuf_get_bytes_written(const xbuf_t *xbuf)
{
	return xbuf->bytes_written;
}

bool_t xbuf_write_to_file(xbuf_t *xbuf, const char *filename, char **out_error)
{
	FILE *fp;
	xbuf_block_t *block;

	if (xbuf->error)
		return (out_error && (*out_error = msprintf("cannot write buffer to file: a previous error occurred"))), false;
	if (xbuf->fp)
		return (out_error && (*out_error = msprintf("xbuf_write_to_file called on a file buffer"))), false;

	fp = openfile_write(filename, out_error);
	if (!fp)
		return false;

	for (block = xbuf->block_head; block; block = block->next)
	{
		if (fwrite(block->memory, 1, block->bytes_written, fp) < block->bytes_written)
		{
			if (out_error)
				*out_error = msprintf("failed to write to file: %s", strerror(errno));
			fclose(fp);
			return false;
		}
	}

	fclose(fp);
	return true;
}

/* return the data in a newly allocated contiguous block, then free the xbuf */
bool_t xbuf_finish_memory(xbuf_t *xbuf, void **out_data, size_t *out_length, char **out_error)
{
	void *data;
	unsigned char *p;
	xbuf_block_t *block;

	if (xbuf->error)
		return xbuf_free(xbuf, out_error);

	data = qmalloc(xbuf->bytes_written);
	if (!data)
	{
		xbuf_free(xbuf, NULL);
		return (out_error && (*out_error = msprintf("out of memory"))), false;
	}

	p = (unsigned char*)data;
	for (block = xbuf->block_head; block; block = block->next)
	{
		memcpy(p, block->memory, block->bytes_written);
		p += block->bytes_written;
	}

	*out_data = data;
	*out_length = xbuf->bytes_written;

	return xbuf_free(xbuf, out_error); /* this will always return true */
}

/* finish writing to file, then free the xbuf */
bool_t xbuf_finish_file(xbuf_t *xbuf, char **out_error)
{
	xbuf_flush(xbuf);

	return xbuf_free(xbuf, out_error);
}
