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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "model.h"

typedef struct md3_vertex_s
{
	short origin[3];
	short normalpitchyaw;
} md3_vertex_t;

typedef struct md3_frameinfo_s
{
	float mins[3];
	float maxs[3];
	float origin[3];
	float radius;
	char name[16];
} md3_frameinfo_t;

typedef struct md3_tag_s
{
	char name[64];
	float origin[3];
	float rotationmatrix[9];
} md3_tag_t;

typedef struct md3_shader_s
{
	char name[64];
	int shadernum; /* not used by the disk format */
} md3_shader_t;

typedef struct md3_header_s
{
	char ident[4]; /* "IDP3" */
	int version; /* 15 */

	char name[64];

	int flags; /* unused by quake3, darkplaces uses it for quake-style modelflags (rocket trails, etc.) */

	int num_frames;
	int num_tags;
	int num_meshes;
	int num_skins;

/* lump offsets are relative to start of header (start of file) */
	int lump_frameinfo;
	int lump_tags;
	int lump_meshes;
	int lump_end;
} md3_header_t;

typedef struct md3_mesh_s
{
	char ident[4]; /* "IDP3" */

	char name[64];

	int flags; /* unused */

	int num_frames;
	int num_shaders;
	int num_vertices;
	int num_triangles;

/* lump offsets are relative to start of mesh */
	int lump_elements;
	int lump_shaders;
	int lump_texcoords;
	int lump_framevertices;
	int lump_end;
} md3_mesh_t;

bool_t model_md3_load(void *filedata, size_t filesize, model_t *out_model, char **out_error)
{
	unsigned char *f = (unsigned char*)filedata;
	md3_header_t *header;
	int i, j;
	model_t model;

	header = (md3_header_t*)f;

/* validate format */
	if (memcmp(header->ident, "IDP3", 4))
		return (out_error && (*out_error = msprintf("wrong format (not IDP3)"))), false;
	if (LittleLong(header->version) != 15)
		return (out_error && (*out_error = msprintf("wrong format (version not 6)"))), false;

/* byteswap header */
	header->version        = LittleLong(header->version);
	header->flags          = LittleLong(header->flags);
	header->num_frames     = LittleLong(header->num_frames);
	header->num_tags       = LittleLong(header->num_tags);
	header->num_meshes     = LittleLong(header->num_meshes);
	header->num_skins      = LittleLong(header->num_skins);
	header->lump_frameinfo = LittleLong(header->lump_frameinfo);
	header->lump_tags      = LittleLong(header->lump_tags);
	header->lump_meshes    = LittleLong(header->lump_meshes);
	header->lump_end       = LittleLong(header->lump_end);

/*	printf("header numskins: %d\n", header->num_skins);*/

/* read skins */
	model.total_skins = 0;
	model.num_skins = 0;
	model.skininfo = NULL;

/* read frames */
	model.total_frames = header->num_frames;

	model.num_frames = header->num_frames;
	model.frameinfo = (frameinfo_t*)qmalloc(sizeof(frameinfo_t) * model.num_frames);

	f = (unsigned char*)filedata + header->lump_frameinfo;
	for (i = 0; i < model.num_frames; i++)
	{
		md3_frameinfo_t *md3_frameinfo = (md3_frameinfo_t*)f;

		model.frameinfo[i].frametime = 0.1f;
		model.frameinfo[i].num_frames = 1;
		model.frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t));
		model.frameinfo[i].frames[0].name = copystring(md3_frameinfo->name);
		model.frameinfo[i].frames[0].offset = i;

		f += sizeof(md3_frameinfo_t);
	}

/* read tags */
	model.num_tags = header->num_tags;
	model.tags = (tag_t*)qmalloc(sizeof(tag_t) * model.num_tags);

	f = (unsigned char*)filedata + header->lump_tags;
	for (i = 0; i < header->num_tags; i++)
		model.tags[i].matrix = (mat4x4f_t*)qmalloc(sizeof(mat4x4f_t) * model.num_frames);
	for (i = 0; i < model.num_frames; i++)
	{
		for (j = 0; j < header->num_tags; j++)
		{
			md3_tag_t *md3_tag = (md3_tag_t*)f;
			tag_t *tag = &model.tags[j];

			if (i == 0)
				tag->name = copystring(md3_tag->name);

			tag->matrix[i].m[0][0] = LittleFloat(md3_tag->rotationmatrix[0]);
			tag->matrix[i].m[0][1] = LittleFloat(md3_tag->rotationmatrix[3]);
			tag->matrix[i].m[0][2] = LittleFloat(md3_tag->rotationmatrix[6]);
			tag->matrix[i].m[0][3] = LittleFloat(md3_tag->origin[0]);
			tag->matrix[i].m[1][0] = LittleFloat(md3_tag->rotationmatrix[1]);
			tag->matrix[i].m[1][1] = LittleFloat(md3_tag->rotationmatrix[4]);
			tag->matrix[i].m[1][2] = LittleFloat(md3_tag->rotationmatrix[7]);
			tag->matrix[i].m[1][3] = LittleFloat(md3_tag->origin[1]);
			tag->matrix[i].m[2][0] = LittleFloat(md3_tag->rotationmatrix[2]);
			tag->matrix[i].m[2][1] = LittleFloat(md3_tag->rotationmatrix[5]);
			tag->matrix[i].m[2][2] = LittleFloat(md3_tag->rotationmatrix[8]);
			tag->matrix[i].m[2][3] = LittleFloat(md3_tag->origin[2]);
			tag->matrix[i].m[3][0] = 0;
			tag->matrix[i].m[3][1] = 0;
			tag->matrix[i].m[3][2] = 0;
			tag->matrix[i].m[3][3] = 1;

			f += sizeof(md3_tag_t);
		}
	}

/* read meshes */
	model.num_meshes = header->num_meshes;
	model.meshes = (mesh_t*)qmalloc(sizeof(mesh_t) * model.num_meshes);

	f = (unsigned char*)filedata + header->lump_meshes;
	for (i = 0; i < header->num_meshes; i++)
	{
		md3_mesh_t *md3_mesh = (md3_mesh_t*)f;
		mesh_t *mesh = &model.meshes[i];
		md3_vertex_t *md3_vertex;

		md3_mesh->flags              = LittleLong(md3_mesh->flags);
		md3_mesh->num_frames         = LittleLong(md3_mesh->num_frames);
		md3_mesh->num_shaders        = LittleLong(md3_mesh->num_shaders);
		md3_mesh->num_vertices       = LittleLong(md3_mesh->num_vertices);
		md3_mesh->num_triangles      = LittleLong(md3_mesh->num_triangles);
		md3_mesh->lump_elements      = LittleLong(md3_mesh->lump_elements);
		md3_mesh->lump_shaders       = LittleLong(md3_mesh->lump_shaders);
		md3_mesh->lump_texcoords     = LittleLong(md3_mesh->lump_texcoords);
		md3_mesh->lump_framevertices = LittleLong(md3_mesh->lump_framevertices);
		md3_mesh->lump_end           = LittleLong(md3_mesh->lump_end);

/*		printf("mesh %d: \"%s\"\n", i, md3_mesh->name);*/

		mesh_initialize(&model, mesh);

		mesh->name = copystring(md3_mesh->name);

		mesh->num_vertices = md3_mesh->num_vertices;
		mesh->num_triangles = md3_mesh->num_triangles;

	/* load triangles */
		mesh->triangle3i = (int*)qmalloc(sizeof(int) * mesh->num_triangles * 3);

		for (j = 0; j < mesh->num_triangles; j++)
		{
			const int *p = (const int*)(f + md3_mesh->lump_elements) + j * 3;
			mesh->triangle3i[j*3+0] = LittleLong(p[0]);
			mesh->triangle3i[j*3+1] = LittleLong(p[1]);
			mesh->triangle3i[j*3+2] = LittleLong(p[2]);
		}

	/* load texcoords */
		mesh->texcoord2f = (float*)qmalloc(sizeof(float) * mesh->num_vertices * 2);

		for (j = 0; j < mesh->num_vertices; j++)
		{
			const float *p = (const float*)(f + md3_mesh->lump_texcoords) + j * 2;
			mesh->texcoord2f[j*2+0] = LittleFloat(p[0]);
			mesh->texcoord2f[j*2+1] = LittleFloat(p[1]);
		}

	/* load frames */
		mesh->vertex3f = (float*)qmalloc(sizeof(float) * model.num_frames * mesh->num_vertices * 3);
		mesh->normal3f = (float*)qmalloc(sizeof(float) * model.num_frames * mesh->num_vertices * 3);

		for (j = 0, md3_vertex = (md3_vertex_t*)(f + md3_mesh->lump_framevertices); j < model.num_frames * mesh->num_vertices; j++, md3_vertex++)
		{
			double npitch, nyaw;

			mesh->vertex3f[j*3+0] = LittleShort(md3_vertex->origin[0]) * (1.0f / 64.0f);
			mesh->vertex3f[j*3+1] = LittleShort(md3_vertex->origin[1]) * (1.0f / 64.0f);
			mesh->vertex3f[j*3+2] = LittleShort(md3_vertex->origin[2]) * (1.0f / 64.0f);

		/* decompress the vertex normal */
			md3_vertex->normalpitchyaw = LittleShort(md3_vertex->normalpitchyaw);
			npitch = (md3_vertex->normalpitchyaw & 255) * (2 * M_PI) / 256.0;
			nyaw = ((md3_vertex->normalpitchyaw >> 8) & 255) * (2 * M_PI) / 256.0;

			mesh->normal3f[j*3+0] = (float)(sin(npitch) * cos(nyaw));
			mesh->normal3f[j*3+1] = (float)(sin(npitch) * sin(nyaw));
			mesh->normal3f[j*3+2] = (float)cos(npitch);
		}

	/* load shaders */
/*		for (j = 0; j < md3_mesh->num_shaders; j++)
		{
			md3_shader_t *md3_shader = (md3_shader_t*)(f + md3_mesh->lump_shaders) + j;

			printf(" - shader %d: \"%s\"\n", j, md3_shader->name);
		}*/

	/* load skin */
		mesh->skins = NULL;

		f += md3_mesh->lump_end;
	}

	model.flags = header->flags;
	model.synctype = 0;
	model.offsets[0] = 0.0f;
	model.offsets[1] = 0.0f;
	model.offsets[2] = 0.0f;

	*out_model = model;
	return true;
}
