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

typedef struct singleframe_s
{
	char *name;
	int offset; /* how many "actual frames" into the vertex3f/normal3f lumps */
} singleframe_t;

typedef struct frameinfo_s
{
	int num_frames;
	singleframe_t *frames;

	float frametime; /* 0.1 for single frames, otherwise based on framegroup interval */
} frameinfo_t;

typedef struct tag_s
{
	char *name;

	mat4x4f_t *matrix;
} tag_t;

typedef struct mesh_s
{
	char *name;

	int num_vertices;
	int num_triangles;

	float *vertex3f;
	float *normal3f;
	float *texcoord2f;

	int *triangle3i;

	image_rgba_t texture_diffuse;
	image_rgba_t texture_fullbright;

	struct
	{
		bool_t initialized;

	/* the texture may need to be padded to power-of-two dimensions to be rendered, so we need extra texturing information */
		float *texcoord2f;

		image_rgba_t texture_diffuse;
		unsigned int texture_diffuse_handle;

		image_rgba_t texture_fullbright;
		unsigned int texture_fullbright_handle;
	} renderdata;
} mesh_t;

typedef struct model_s
{
	int total_frames; /* including framegroup frames */

	int num_frames;
	frameinfo_t *frameinfo;

	int num_meshes;
	mesh_t *meshes;

	int num_tags;
	tag_t *tags;

	int flags; /* quake only */
	int synctype; /* quake only */
} model_t;

void mesh_initialize(mesh_t *mesh);
void mesh_free(mesh_t *mesh);
void mesh_generaterenderdata(mesh_t *mesh);
void mesh_freerenderdata(mesh_t *mesh);

void model_initialize(model_t *model);
void model_free(model_t *model);
void model_generaterenderdata(model_t *model);
void model_freerenderdata(model_t *model);

/* note that the filedata pointer is not const, because it may be modified (most likely by byteswapping) */
bool_t model_load(const char *filename, void *filedata, size_t filesize, model_t *out_model, char **out_error);

bool_t model_mdl_load(void *filedata, size_t filesize, model_t *out_model, char **out_error);
bool_t model_md2_load(void *filedata, size_t filesize, model_t *out_model, char **out_error);
bool_t model_md3_load(void *filedata, size_t filesize, model_t *out_model, char **out_error);

bool_t model_mdl_save(const model_t *model, void **out_data, size_t *out_size);

mesh_t *model_merge_meshes(model_t *model);

#endif
