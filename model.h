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

#ifndef MODEL_H
#define MODEL_H

#include <stdlib.h>

#include "matrix.h"
#include "image.h"

typedef struct frameinfo_s
{
	char *name;
} frameinfo_t;

typedef struct tag_s
{
	char *name;

	mat4x4f_t *matrix; /* one per frame */
} tag_t;

typedef struct mesh_s
{
	char *name;

	int num_vertices;
	int num_triangles;

	float *vertex3f; /* full set per frame */
	float *normal3f; /* full set per frame */
	float *texcoord2f;
	float *paddedtexcoord2f;

	int *triangle3i;

	image_rgba_t texture;
	image_rgba_t paddedtexture;

	unsigned int texture_handle; /* hardware handle */
} mesh_t;

typedef struct model_s
{
	int num_frames;
	frameinfo_t *frameinfo;

	int num_meshes;
	mesh_t *meshes;

	int num_tags;
	tag_t *tags;

	int flags; /* quake only */
	int synctype; /* quake only */
} model_t;

extern const float anorms[162][3];
int compress_normal(const float *normal);

/* note that the filedata pointer is not const, because it may be modified (most likely by byteswapping) */
bool_t model_load(const char *filename, void *filedata, size_t filesize, model_t *out_model);

bool_t model_mdl_load(void *filedata, size_t filesize, model_t *out_model);
bool_t model_md2_load(void *filedata, size_t filesize, model_t *out_model);
bool_t model_md3_load(void *filedata, size_t filesize, model_t *out_model);

bool_t model_mdl_save(const model_t *model, void **out_data, size_t *out_size);

void model_free(model_t *model);

mesh_t *model_merge_meshes(model_t *model);

#endif
