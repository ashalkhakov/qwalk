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

static palette_t quake2palette =
{
	{
		  0,  0,  0,  15, 15, 15,  31, 31, 31,  47, 47, 47,  63, 63, 63,  75, 75, 75,  91, 91, 91, 107,107,107, 123,123,123, 139,139,139, 155,155,155, 171,171,171, 187,187,187, 203,203,203, 219,219,219, 235,235,235,
		 99, 75, 35,  91, 67, 31,  83, 63, 31,  79, 59, 27,  71, 55, 27,  63, 47, 23,  59, 43, 23,  51, 39, 19,  47, 35, 19,  43, 31, 19,  39, 27, 15,  35, 23, 15,  27, 19, 11,  23, 15, 11,  19, 15,  7,  15, 11,  7,
		 95, 95,111,  91, 91,103,  91, 83, 95,  87, 79, 91,  83, 75, 83,  79, 71, 75,  71, 63, 67,  63, 59, 59,  59, 55, 55,  51, 47, 47,  47, 43, 43,  39, 39, 39,  35, 35, 35,  27, 27, 27,  23, 23, 23,  19, 19, 19,
		143,119, 83, 123, 99, 67, 115, 91, 59, 103, 79, 47, 207,151, 75, 167,123, 59, 139,103, 47, 111, 83, 39, 235,159, 39, 203,139, 35, 175,119, 31, 147, 99, 27, 119, 79, 23,  91, 59, 15,  63, 39, 11,  35, 23,  7,
		167, 59, 43, 159, 47, 35, 151, 43, 27, 139, 39, 19, 127, 31, 15, 115, 23, 11, 103, 23,  7,  87, 19,  0,  75, 15,  0,  67, 15,  0,  59, 15,  0,  51, 11,  0,  43, 11,  0,  35, 11,  0,  27,  7,  0,  19,  7,  0,
		123, 95, 75, 115, 87, 67, 107, 83, 63, 103, 79, 59,  95, 71, 55,  87, 67, 51,  83, 63, 47,  75, 55, 43,  67, 51, 39,  63, 47, 35,  55, 39, 27,  47, 35, 23,  39, 27, 19,  31, 23, 15,  23, 15, 11,  15, 11,  7,
		111, 59, 23,  95, 55, 23,  83, 47, 23,  67, 43, 23,  55, 35, 19,  39, 27, 15,  27, 19, 11,  15, 11,  7, 179, 91, 79, 191,123,111, 203,155,147, 215,187,183, 203,215,223, 179,199,211, 159,183,195, 135,167,183,
		115,151,167,  91,135,155,  71,119,139,  47,103,127,  23, 83,111,  19, 75,103,  15, 67, 91,  11, 63, 83,   7, 55, 75,   7, 47, 63,   7, 39, 51,   0, 31, 43,   0, 23, 31,   0, 15, 19,   0,  7, 11,   0,  0,  0,
		139, 87, 87, 131, 79, 79, 123, 71, 71, 115, 67, 67, 107, 59, 59,  99, 51, 51,  91, 47, 47,  87, 43, 43,  75, 35, 35,  63, 31, 31,  51, 27, 27,  43, 19, 19,  31, 15, 15,  19, 11, 11,  11,  7,  7,   0,  0,  0,
		151,159,123, 143,151,115, 135,139,107, 127,131, 99, 119,123, 95, 115,115, 87, 107,107, 79,  99, 99, 71,  91, 91, 67,  79, 79, 59,  67, 67, 51,  55, 55, 43,  47, 47, 35,  35, 35, 27,  23, 23, 19,  15, 15, 11,
		159, 75, 63, 147, 67, 55, 139, 59, 47, 127, 55, 39, 119, 47, 35, 107, 43, 27,  99, 35, 23,  87, 31, 19,  79, 27, 15,  67, 23, 11,  55, 19, 11,  43, 15,  7,  31, 11,  7,  23,  7,  0,  11,  0,  0,   0,  0,  0,
		119,123,207, 111,115,195, 103,107,183,  99, 99,167,  91, 91,155,  83, 87,143,  75, 79,127,  71, 71,115,  63, 63,103,  55, 55, 87,  47, 47, 75,  39, 39, 63,  35, 31, 47,  27, 23, 35,  19, 15, 23,  11,  7,  7,
		155,171,123, 143,159,111, 135,151, 99, 123,139, 87, 115,131, 75, 103,119, 67,  95,111, 59,  87,103, 51,  75, 91, 39,  63, 79, 27,  55, 67, 19,  47, 59, 11,  35, 47,  7,  27, 35,  0,  19, 23,  0,  11, 15,  0,
		  0,255,  0,  35,231, 15,  63,211, 27,  83,187, 39,  95,167, 47,  95,143, 51,  95,123, 51, 255,255,255, 255,255,211, 255,255,167, 255,255,127, 255,255, 83, 255,255, 39, 255,235, 31, 255,215, 23, 255,191, 15,
		255,171,  7, 255,147,  0, 239,127,  0, 227,107,  0, 211, 87,  0, 199, 71,  0, 183, 59,  0, 171, 43,  0, 155, 31,  0, 143, 23,  0, 127, 15,  0, 115,  7,  0,  95,  0,  0,  71,  0,  0,  47,  0,  0,  27,  0,  0,
		239,  0,  0,  55, 55,255, 255,  0,  0,   0,  0,255,  43, 43, 35,  27, 27, 23,  19, 19, 15, 235,151,127, 195,115, 83, 159, 87, 51, 123, 63, 27, 235,211,199, 199,171,155, 167,139,119, 135,107, 87, 159, 91, 83
	},
	/* i don't think quake2 uses fullbrights */
	{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }
};

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

/* skip glcmds (FIXME - they have to be swapped when we export the model) */

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
	typedef struct md2_meshvert_s
	{
		unsigned short vertex;
		unsigned short texcoord;
	} md2_meshvert_t;

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
		model.frameinfo[i].frames[0].name = copystring(frame->name);
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

	mesh->name = copystring("md2mesh");

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

/* FIXME - load skin? */

	*out_model = model;
	return true;
}

bool_t model_md2_save(const model_t *model, void **out_data, size_t *out_size)
{
	const char *skinfilename = "skin.pcx"; /* FIXME */
	char *error;
	unsigned char *data;
	size_t imagesize;
	const mesh_t *mesh;
	md2_header_t *header;
	image_paletted_t *pimage;
	int offset;
	int i, j, k;
	md2_skin_t *md2skin;

	if (model->num_meshes != 1)
	{
	/* FIXME - create a function to combine meshes so that this can be done */
		printf("model_md2_save: model must have only one mesh\n");
		return false;
	}

	/* FIXME - resample fullbright skin to match diffuse skin size */

	mesh = &model->meshes[0];

/* allocate memory for writing */
	data = qmalloc(16*1024*1024); /* FIXME */

/* create 8-bit texture */
	pimage = image_palettize(&quake2palette, mesh->skins[SKIN_DIFFUSE], mesh->skins[SKIN_FULLBRIGHT]);

	imagesize = image_pcx_save(pimage, data, 16*1024*1024);
	if (!imagesize)
	{
		printf("model_md2_save: failed to create pcx\n");
		return false;
	}

	if (!writefile(skinfilename, data, imagesize, &error))
	{
		printf("model_md2_save: failed to write %s: %s\n", skinfilename, error);
		qfree(error);
		return false;
	}

/* write header */
	offset = 0;

	header = (md2_header_t*)(data + offset);

	memcpy(header->ident, "IDP2", 4);
	header->version = 8;
	header->skinwidth = pimage->width;
	header->skinheight = pimage->height;
	header->framesize = sizeof(md2_frame_t) + sizeof(md2_vertex_t) * (mesh->num_vertices - 1); /* subtract one because md2_frame_t has an array of one */
	header->num_skins = 1;
	header->num_vertices = mesh->num_vertices;
	header->num_st = mesh->num_vertices;
	header->num_tris = mesh->num_triangles;
	header->num_glcmds = 0;
	header->num_frames = model->num_frames;
	header->offset_skins = 0;
	header->offset_st = 0;
	header->offset_tris = 0;
	header->offset_frames = 0;
	header->offset_glcmds = 0;
	header->offset_end = 0;

	offset += sizeof(md2_header_t);

/* write skins */
	header->offset_skins = offset;

	md2skin = (md2_skin_t*)(data + offset);
	strlcpy(md2skin->name, skinfilename, sizeof(md2skin->name));

	offset += sizeof(md2_skin_t);

/* write texcoords */
	header->offset_st = offset;

	for (i = 0; i < mesh->num_vertices; i++)
	{
		md2_texcoord_t *md2texcoord = (md2_texcoord_t*)(data + offset);

		md2texcoord->s = (int)(mesh->texcoord2f[i*2+0] * header->skinwidth);
		md2texcoord->t = (int)(mesh->texcoord2f[i*2+1] * header->skinheight);

		offset += sizeof(md2_texcoord_t);
	}

/* write triangles */
	header->offset_tris = offset;

	for (i = 0; i < mesh->num_triangles; i++)
	{
		md2_triangle_t *md2triangle = (md2_triangle_t*)(data + offset);

		for (j = 0; j < 3; j++)
		{
			md2triangle->vertex[j] = mesh->triangle3i[i*3+j];
			md2triangle->st[j] = mesh->triangle3i[i*3+j];
		}

		offset += sizeof(md2_triangle_t);
	}

/* write frames */
	header->offset_frames = offset;

	for (i = 0; i < model->num_frames; i++)
	{
		md2_frame_t *md2frame = (md2_frame_t*)(data + offset);
		float mins[3], maxs[3];
		const float *v, *n;

		strlcpy(md2frame->name, model->frameinfo[i].frames[0].name, sizeof(md2frame->name));

	/* calculate bounds of frame */
		mins[0] = mins[1] = mins[2] = maxs[0] = maxs[1] = maxs[2] = 0.0f;
		v = mesh->vertex3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		for (j = 0; j < mesh->num_vertices; j++, v += 3)
		{
			for (k = 0; k < 3; k++)
			{
				mins[k] = min(mins[k], v[k]);
				maxs[k] = max(maxs[k], v[k]);
			}
		}

		for (j = 0; j < 3; j++)
		{
			md2frame->scale[j] = (maxs[j] - mins[j]) * (1.0f / 255.0f);
			md2frame->translate[j] = mins[j];
		}

	/* write vertices */
		v = mesh->vertex3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		n = mesh->normal3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		for (j = 0; j < mesh->num_vertices; j++, v += 3, n += 3)
		{
			for (k = 0; k < 3; k++)
			{
				float pos = (v[k] - md2frame->translate[k]) / md2frame->scale[k];

				pos = (float)floor(pos + 0.5f);
				pos = bound(0.0f, pos, 255.0f);

				md2frame->verts[j].v[k] = (unsigned char)pos;
			}

			md2frame->verts[j].lightnormalindex = compress_normal(n);
		}

		offset += header->framesize;
	}

/* write glcmds */
	header->offset_glcmds = offset;

/* write end */
	header->offset_end = offset;

/* done */
	swap_md2(data, offset);
	*out_data = data;
	*out_size = offset;
	return true;
}
