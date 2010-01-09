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
#include "model.h"

extern const float anorms[162][3];
int compress_normal(const float *normal);

typedef struct md2_header_s
{
	char ident[4];
	int version;

	int skinwidth;
	int skinheight;

	int framesize;

	int num_skins;
	int num_vertices;
	int num_st;
	int num_tris;
	int num_glcmds;
	int num_frames;

	int offset_skins;
	int offset_st;
	int offset_tris;
	int offset_frames;
	int offset_glcmds;
	int offset_end;
} md2_header_t;

typedef struct md2_skin_s
{
	char name[64];
} md2_skin_t;

typedef struct md2_texcoord_s
{
	short s;
	short t;
} md2_texcoord_t;

typedef struct md2_triangle_s
{
	unsigned short vertex[3];
	unsigned short st[3];
} md2_triangle_t;

typedef struct md2_vertex_s
{
	unsigned char v[3];
	unsigned char lightnormalindex;
} md2_vertex_t;

typedef struct md2_frame_s
{
	float scale[3];
	float translate[3];
	char name[16];
	md2_vertex_t verts[1];
} md2_frame_t;

typedef struct md2_meshvert_s
{
	unsigned short vertex; /* index to md2_vertex_t */
	unsigned short texcoord; /* index to md2_texcoord_t */
} md2_meshvert_t;

static void swap_md2(void *filedata, size_t filesize)
{
	unsigned char *f = (unsigned char*)filedata;
	md2_header_t *header;
	int i;

	header = (md2_header_t*)f;

	header->version       = LittleLong(header->version);
	header->skinwidth     = LittleLong(header->skinwidth);
	header->skinheight    = LittleLong(header->skinheight);
	header->framesize     = LittleLong(header->framesize);
	header->num_skins     = LittleLong(header->num_skins);
	header->num_vertices  = LittleLong(header->num_vertices);
	header->num_st        = LittleLong(header->num_st);
	header->num_tris      = LittleLong(header->num_tris);
	header->num_glcmds    = LittleLong(header->num_glcmds);
	header->num_frames    = LittleLong(header->num_frames);
	header->offset_skins  = LittleLong(header->offset_skins);
	header->offset_st     = LittleLong(header->offset_st);
	header->offset_tris   = LittleLong(header->offset_tris);
	header->offset_frames = LittleLong(header->offset_frames);
	header->offset_glcmds = LittleLong(header->offset_glcmds);
	header->offset_end    = LittleLong(header->offset_end);

/* skip skins (they're only strings so they aren't swapped) */

/* swap texcoords */
	for (i = 0; i < header->num_st; i++)
	{
		md2_texcoord_t *texcoord = (md2_texcoord_t*)(f + header->offset_st) + i;
		texcoord->s = LittleShort(texcoord->s);
		texcoord->t = LittleShort(texcoord->t);
	}

/* swap triangles */
	for (i = 0; i < header->num_tris; i++)
	{
		md2_triangle_t *triangle = (md2_triangle_t*)(f + header->offset_tris) + i;
		triangle->vertex[0] = LittleShort(triangle->vertex[0]);
		triangle->vertex[1] = LittleShort(triangle->vertex[1]);
		triangle->vertex[2] = LittleShort(triangle->vertex[2]);
		triangle->st[0] = LittleShort(triangle->st[0]);
		triangle->st[1] = LittleShort(triangle->st[1]);
		triangle->st[2] = LittleShort(triangle->st[2]);
	}

/* skip glcmds (FIXME) */

/* read frames */
	f += header->offset_frames;

	for (i = 0; i < header->num_frames; i++)
	{
		md2_frame_t *frame = (md2_frame_t*)f;

		frame->scale[0] = LittleFloat(frame->scale[0]);
		frame->scale[1] = LittleFloat(frame->scale[1]);
		frame->scale[2] = LittleFloat(frame->scale[2]);
		frame->translate[0] = LittleFloat(frame->translate[0]);
		frame->translate[1] = LittleFloat(frame->translate[1]);
		frame->translate[2] = LittleFloat(frame->translate[2]);

		f += header->framesize;
	}
}

bool_t model_md2_load(void *filedata, size_t filesize, model_t *out_model, char **out_error)
{
	unsigned char *f = (unsigned char*)filedata;
	md2_header_t *header;
	int i, j;
	md2_texcoord_t *texcoords;
	md2_triangle_t *triangles;
	model_t model;
	mesh_t *mesh;
	md2_meshvert_t *meshverts;
	float iwidth, iheight;

	header = (md2_header_t*)f;

/* validate format */
	if (memcmp(header->ident, "IDP2", 4))
	{
		if (out_error)
			*out_error = msprintf("wrong format (not IDP2)");
		return false;
	}
	if (LittleLong(header->version) != 8)
	{
		if (out_error)
			*out_error = msprintf("wrong format (version not 8)");
		return false;
	}

/* byteswap header */
	swap_md2(filedata, filesize);

/* stuff */
	texcoords = (md2_texcoord_t*)(f + header->offset_st);
	triangles = (md2_triangle_t*)(f + header->offset_tris);

/* stuff */
	model.total_frames = header->num_frames;

	model.num_frames = header->num_frames;
	model.frameinfo = (frameinfo_t*)qmalloc(sizeof(frameinfo_t) * model.num_frames);

	for (i = 0; i < model.num_frames; i++)
	{
		md2_frame_t *frame = (md2_frame_t*)(f + header->offset_frames + i * header->framesize);

		model.frameinfo[i].frametime = 0.1f;
		model.frameinfo[i].num_frames = 1;
		model.frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t));
		model.frameinfo[i].frames[0].name = (char*)qmalloc(strlen(frame->name) + 1);
		strcpy(model.frameinfo[i].frames[0].name, frame->name);
		model.frameinfo[i].frames[0].offset = i;
	}

	model.num_meshes = 1;
	model.meshes = (mesh_t*)qmalloc(sizeof(mesh_t));

	model.num_tags = 0;
	model.tags = NULL;

	model.flags = 0;
	model.synctype = 0;

	mesh = &model.meshes[0];
	mesh_initialize(mesh);

	mesh->name = (char*)qmalloc(strlen("md2mesh") + 1);
	strcpy(mesh->name, "md2mesh");

	mesh->num_triangles = header->num_tris;
	mesh->triangle3i = (int*)qmalloc(sizeof(int) * mesh->num_triangles * 3);

	mesh->num_vertices = 0;
	meshverts = (md2_meshvert_t*)qmalloc(sizeof(md2_meshvert_t) * mesh->num_triangles * 3);
	for (i = 0; i < mesh->num_triangles; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int vertnum;
			unsigned short xyz = triangles[i].vertex[j];
			unsigned short st = triangles[i].st[j];

			for (vertnum = 0; vertnum < mesh->num_vertices; vertnum++)
			{
			/* see if this xyz+st combination exists in the buffer yet */
				md2_meshvert_t *mv = &meshverts[vertnum];

				if (mv->vertex == xyz && mv->texcoord == st)
					break;
			}
			if (vertnum == mesh->num_vertices)
			{
			/* it isn't in the list yet, add it */
				meshverts[vertnum].vertex = xyz;
				meshverts[vertnum].texcoord = st;
				mesh->num_vertices++;
			}

			mesh->triangle3i[i * 3 + j] = vertnum; /* (clockwise winding) */
		}
	}

	mesh->texcoord2f = (float*)qmalloc(sizeof(float) * mesh->num_vertices * 2);
	iwidth = 1.0f / header->skinwidth;
	iheight = 1.0f / header->skinheight;
	for (i = 0; i < mesh->num_vertices; i++)
	{
		mesh->texcoord2f[i*2+0] = (texcoords[meshverts[i].texcoord].s + 0.5f) * iwidth;
		mesh->texcoord2f[i*2+1] = (texcoords[meshverts[i].texcoord].t + 0.5f) * iheight;
	}

	mesh->vertex3f = (float*)qmalloc(model.num_frames * sizeof(float) * mesh->num_vertices * 3);
	mesh->normal3f = (float*)qmalloc(model.num_frames * sizeof(float) * mesh->num_vertices * 3);
	f += header->offset_frames;
	for (i = 0; i < model.num_frames; i++)
	{
		md2_frame_t *frame = (md2_frame_t*)f;
		int firstvertex = i * mesh->num_vertices * 3;

		for (j = 0; j < mesh->num_vertices; j++)
		{
			md2_vertex_t *vtx = &frame->verts[meshverts[j].vertex];
			float *v = mesh->vertex3f + firstvertex + j * 3;
			float *n = mesh->normal3f + firstvertex + j * 3;

			v[0] = frame->translate[0] + frame->scale[0] * vtx->v[0];
			v[1] = frame->translate[1] + frame->scale[1] * vtx->v[1];
			v[2] = frame->translate[2] + frame->scale[2] * vtx->v[2];

			n[0] = anorms[vtx->lightnormalindex][0];
			n[1] = anorms[vtx->lightnormalindex][1];
			n[2] = anorms[vtx->lightnormalindex][2];
		}

		f += header->framesize;
	}

	qfree(meshverts);

/* FIXME? */
	mesh->texture_diffuse.width = 0;
	mesh->texture_diffuse.height = 0;
	mesh->texture_diffuse.pixels = NULL;

	mesh->texture_fullbright.width = 0;
	mesh->texture_fullbright.height = 0;
	mesh->texture_fullbright.pixels = NULL;

	*out_model = model;
	return true;
}
