/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.
Modified to fit into QWalk.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include <ctype.h>
#include <string.h>
#include "global.h"
#include "shaders.h"
#include "image.h"
#include "util.h"

/*
===========================================================================

PARSING

===========================================================================
*/

#define MAX_TOKEN_CHARS 1024
static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;
static	int		com_tokenline;

static void COM_BeginParseSession(const char *name)
{
	com_lines = 1;
	com_tokenline = 0;
	snprintf(com_parsename, sizeof(com_parsename), "%s", name);
}

static int COM_GetCurrentParseLine(void)
{
	if (com_tokenline)
	{
		return com_tokenline;
	}

	return com_lines;
}

static char *SkipWhitespace(char *data, int *linesSkipped)
{
	int c;

	while ((c = *data) <= ' ')
    {
		if (!c)
        {
			return NULL;
		}
		if (c == '\n')
        {
			*linesSkipped += 1;
		}
		data++;
	}

	return data;
}

static char *COM_ParseExt2(char **data_p, bool_t allowLineBreaks, char delimiter)
{
	int c = 0, len;
	int linesSkipped = 0;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	com_tokenline = 0;

	// make sure incoming data is valid
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}

	while (1)
	{
		// skip whitespace
		data = SkipWhitespace(data, &linesSkipped);
		if (!data)
		{
			*data_p = NULL;
			return com_token;
		}
		if (data && linesSkipped && !allowLineBreaks)
		{
			// ZTM: Don't move the pointer so that calling SkipRestOfLine afterwards works as expected
			//*data_p = data;
			return com_token;
		}

		com_lines += linesSkipped;

		c = *data;

		// skip double slash comments
		if (c == '/' && data[1] == '/')
		{
			data += 2;
			while (*data && *data != '\n')
            {
				data++;
			}
		}
		// skip /* */ comments
		else if (c=='/' && data[1] == '*')
		{
			data += 2;
			while (*data && (*data != '*' || data[1] != '/'))
			{
				if (*data == '\n')
				{
					com_lines++;
				}
				data++;
			}
			if (*data)
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// token starts on this line
	com_tokenline = com_lines;

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = (char *)data;
				return com_token;
			}
			if (c == '\n')
			{
				com_lines++;
			}
			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32 && c != delimiter);

	com_token[len] = 0;

	*data_p = (char *)data;
	return com_token;
}

static char *COM_ParseExt(char **data_p, bool_t allowLineBreaks)
{
	return COM_ParseExt2(data_p, allowLineBreaks, 0);
}

static bool_t SkipBracedSection(char **program, int depth)
{
	char			*token;

	do
    {
		token = COM_ParseExt(program, true);
		if (token[1] == 0)
        {
			if (token[0] == '{')
            {
				depth++;
			}
			else if (token[0] == '}')
            {
				depth--;
			}
		}
	} while (depth && *program);

	return (depth == 0);
}

/*
===========================================================================

SHADER GENERATION

===========================================================================
*/

static int initialized = 0;

#define MAX_QPATH 64

static char shader_base_path[1024];

#define MAX_SHADERS 8192

typedef struct shader_source_s shader_source_t;
typedef struct shader_s shader_t;

typedef struct shader_source_s
{
    char            filename[MAX_QPATH];
    bool_t          sourced;
    shader_t        *shaders, *new_shaders;
    shader_source_t *next;
} shader_source_t;

typedef struct shader_s
{
    char             name[MAX_QPATH];
    bool_t           sourced;

    char             diffuse_map[MAX_QPATH];
    int              alpha_tested;
    char             fullbright_map[MAX_QPATH];

    shader_source_t  *file;
    shader_t         *next, *next_in_file;
} shader_t;

#define	MAX_SHADER_FILES	4096
#define FILE_HASH_SIZE		1024

static  shader_source_t     *source_hash_table[FILE_HASH_SIZE];
static shader_source_t      shader_sources[MAX_SHADER_FILES];
static int                  num_shader_sources;

static shader_t	        	*hash_table[FILE_HASH_SIZE];
static shader_t             shaders[MAX_SHADERS];
static int                  num_shaders;

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname, const int size ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		if (letter == '/') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}

static shader_source_t *FindShaderSourceByName(const char *name)
{
	int			    hash;
	shader_source_t	*source;

    if (!name || !name[0])
        return NULL;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	//
	// see if the shader source is already loaded
	//
	for (source=source_hash_table[hash]; source; source=source->next) {
		if (strcasecmp(source->filename, name) == 0) {
			// match found
			return source;
		}
	}

    if (num_shader_sources == MAX_SHADER_FILES)
    {
        return NULL;
    }
	source = &shader_sources[num_shader_sources++];
    Q_strlcpy(source->filename, name, sizeof(source->filename));
    source->shaders = NULL;
    source->new_shaders = NULL;
	hash = generateHashValue(source->filename, FILE_HASH_SIZE);
	source->next = source_hash_table[hash];
	source_hash_table[hash] = source;
}

static shader_t *FindShaderByName(const char *name)
{
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ((name==NULL) || (name[0] == 0))
    {
		return NULL;
	}

	strip_extension(name, strippedName);

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh=hash_table[hash]; sh; sh=sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (strcasecmp(sh->name, strippedName) == 0) {
			// match found
			return sh;
		}
	}

    if (num_shaders == MAX_SHADERS)
    {
        return NULL;
    }
	sh = &shaders[num_shaders++];
    Q_strlcpy(sh->name, strippedName, sizeof(sh->name));
	hash = generateHashValue(sh->name, FILE_HASH_SIZE);
	sh->next = hash_table[hash];
	hash_table[hash] = sh;
	sh->next_in_file = NULL;
    sh->file = NULL;

	return sh;
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static bool_t ScanAndLoadShaderFiles(char **out_error)
{
    char shader_dir[1024];
	char **shaderFiles;
	char *buffer;
	char *p;
	int numShaderFiles;
	int i;
	char *oldp, *token, *hashMem;
	int hash;
    size_t size;
    char shaderName[MAX_QPATH];
    int shaderLine;
    shader_t *shader;
    shader_source_t *shader_source;

	// scan for shader files
    snprintf(shader_dir, sizeof(shader_dir), "%s", shader_base_path);
	shaderFiles = list_files(shader_dir, ".shader", &numShaderFiles);

	if (!shaderFiles || !numShaderFiles)
	{
        return (void)(out_error && (*out_error = msprintf("no shader files found"))), false;
	}

	if (numShaderFiles > MAX_SHADER_FILES)
    {
		numShaderFiles = MAX_SHADER_FILES;
	}
    
	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		char *filename = msprintf("%s/%s", shader_dir, shaderFiles[i]);

        shader_source = FindShaderSourceByName(shaderFiles[i]);
        if (!shader_source)
        {
            break;
        }
        shader_source->sourced = true;

        if (!loadfile(filename, (void **)&buffer, &size, out_error))
        {
            break;
        }

		p = buffer;
		COM_BeginParseSession(filename);
		while (1)
		{
			token = COM_ParseExt(&p, true);
			
			if(!*token)
				break;

			Q_strlcpy(shaderName, token, sizeof(shaderName));
			shaderLine = COM_GetCurrentParseLine();

			token = COM_ParseExt(&p, true);
			if(token[0] != '{' || token[1] != '\0')
			{
				printf("WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing opening brace",
							filename, shaderName, shaderLine);
				if (token[0])
				{
					printf(" (found \"%s\" on line %d)", token, COM_GetCurrentParseLine());
				}
				printf(".\n");
				break;
			}

			if(!SkipBracedSection(&p, 1))
			{
				printf("WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing closing brace.\n",
							filename, shaderName, shaderLine);
				break;
			}

            shader = FindShaderByName(shaderName);
            if (shader == NULL)
            {
				printf("not enough space\n");
                break; // not enough space
            }

            if (shader->file != NULL)
            {
                // it's a duplicate sourced shader...
                //printf("Shader \"%s\" on line %d of file %s previously defined in file %s.\n",
                //    shaderName, shaderLine, shader->file->filename);
            }
            else
            {
                shader->sourced = true;
                shader->file = shader_source;
                shader->next_in_file = shader_source->shaders;
                shader_source->shaders = shader;
            }
		}

		if (buffer)
		{
			qfree(buffer);
			buffer = NULL;
		}
	}

	// free up memory
	free_list_files(shaderFiles, numShaderFiles);
}

// ==========================================

bool_t init_shaders(const char *basepath, char **out_error)
{
    bool_t success;

    if (initialized || !basepath || !*basepath)
        return true;

    Q_strlcpy(shader_base_path,  basepath, sizeof(shader_base_path));

	memset(hash_table, 0, sizeof(hash_table));
    memset(source_hash_table, 0, sizeof(source_hash_table));

	success = ScanAndLoadShaderFiles(out_error);

    initialized = 1;

    return success;
}

void define_shader(const char *shader_source_name, const char *name, const char *diffuse_image, const char *fullbright_image, bool_t alpha_tested)
{
    shader_t        *sh;
    shader_source_t *shader_source;
    int             i;

    if (!initialized)
        return;

    if (!alpha_tested && !(fullbright_image && fullbright_image[0]))
        return; // not interesting enough

    sh = FindShaderByName(name);
    if (sh == NULL)
    {
        // can't doo much
        return;
    }
    if (sh->sourced)
    {
        return;
    }
    Q_strlcpy(sh->diffuse_map, diffuse_image, sizeof(sh->diffuse_map));
    sh->alpha_tested = alpha_tested;
    if (fullbright_image && fullbright_image[0])
    {
        Q_strlcpy(sh->fullbright_map, fullbright_image, sizeof(sh->fullbright_map));
    }
    else
    {
        sh->fullbright_map[0] = 0;
    }

    shader_source = FindShaderSourceByName(shader_source_name);

    sh->sourced = false;
    sh->file = shader_source;
    sh->next_in_file = shader_source->new_shaders;
    shader_source->new_shaders = sh;
}

static void fprint_shader(FILE *fp, shader_t *shader)
{
    fprintf(fp, "\n%s\n", shader->name);
    fprintf(fp, "{\n");
    fprintf(fp, "\tsurfaceparm alphashadow\n");
    fprintf(fp, "\tsurfaceparm trans\n");
    fprintf(fp, "\t{\n");
    fprintf(fp, "\t\tmap %s\n", shader->diffuse_map);
    fprintf(fp, "\t\trgbGen identity\n");
    fprintf(fp, "\t\tdepthWrite\n");
    fprintf(fp, "\t\talphaFunc GE128\n");
    fprintf(fp, "\t}\n");
    fprintf(fp, "}\n");
}

void write_shaders(void)
{
    int             i;
    shader_source_t *source;
    shader_t        *shader;
    FILE            *fp;
    char            shader_file_name[1024];

    if (!initialized)
        return;

    // go over all files that have shaders that need writing
    // append each shader to its file
    for (i = 0; i < num_shader_sources; i++)
    {
        source = &shader_sources[i];
        shader = source->new_shaders;

        if (!shader)
        {
            continue;
        }

        snprintf(shader_file_name, sizeof(shader_file_name), "%s/%s", shader_base_path, source->filename);

        if (!(fp = fopen(shader_file_name, "ab")))
        {
            // unable to open the file for writing
            continue;
        }

        while (shader)
        {
            fprint_shader(fp, shader);
            shader = shader->next;
        }

        fclose(fp);
    }
}
