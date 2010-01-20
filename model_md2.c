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

extern int texwidth, texheight;
extern const char *g_skinpath;

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

typedef struct dstvert_s
{
	short s;
	short t;
} dstvert_t;

typedef struct dtriangle_s
{
	unsigned short index_xyz[3];
	unsigned short index_st[3];
} dtriangle_t;

typedef struct dtrivertx_s
{
	unsigned char v[3];
	unsigned char lightnormalindex;
} dtrivertx_t;

typedef struct daliasframe_s
{
	float scale[3];
	float translate[3];
	char name[16];
	dtrivertx_t verts[1];
} daliasframe_t;

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
		dstvert_t *texcoord = (dstvert_t*)(f + header->offset_st) + i;
		texcoord->s = LittleShort(texcoord->s);
		texcoord->t = LittleShort(texcoord->t);
	}

/* swap triangles */
	for (i = 0; i < header->num_tris; i++)
	{
		dtriangle_t *triangle = (dtriangle_t*)(f + header->offset_tris) + i;
		triangle->index_xyz[0] = LittleShort(triangle->index_xyz[0]);
		triangle->index_xyz[1] = LittleShort(triangle->index_xyz[1]);
		triangle->index_xyz[2] = LittleShort(triangle->index_xyz[2]);
		triangle->index_st[0] = LittleShort(triangle->index_st[0]);
		triangle->index_st[1] = LittleShort(triangle->index_st[1]);
		triangle->index_st[2] = LittleShort(triangle->index_st[2]);
	}

/* swap glcmds */
	for (i = 0; i < header->num_glcmds; i++)
	{
		int *glcmd = (int*)(f + header->offset_glcmds) + i;
		*glcmd = LittleLong(*glcmd);
	}

/* read frames */
	f += header->offset_frames;

	for (i = 0; i < header->num_frames; i++)
	{
		daliasframe_t *frame = (daliasframe_t*)f;

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
	const md2_skin_t *md2skins;
	const dstvert_t *dstverts;
	const dtriangle_t *dtriangles;
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

/* byteswap file */
	swap_md2(filedata, filesize);

/* stuff */
	md2skins = (md2_skin_t*)(f + header->offset_skins);
	dstverts = (dstvert_t*)(f + header->offset_st);
	dtriangles = (dtriangle_t*)(f + header->offset_tris);

/* stuff */
	model.total_skins = header->num_skins;
	model.num_skins = header->num_skins;
	model.skininfo = (skininfo_t*)qmalloc(sizeof(skininfo_t) * model.num_skins);

	for (i = 0; i < model.num_skins; i++)
	{
		model.skininfo[i].frametime = 0.1f;
		model.skininfo[i].num_skins = 1;
		model.skininfo[i].skins = (singleskin_t*)qmalloc(sizeof(skininfo_t));
		model.skininfo[i].skins[0].name = copystring(md2skins[i].name);
		model.skininfo[i].skins[0].offset = i;
	}

	model.total_frames = header->num_frames;
	model.num_frames = header->num_frames;
	model.frameinfo = (frameinfo_t*)qmalloc(sizeof(frameinfo_t) * model.num_frames);

	for (i = 0; i < model.num_frames; i++)
	{
		const daliasframe_t *md2frame = (const daliasframe_t*)(f + header->offset_frames + i * header->framesize);

		model.frameinfo[i].frametime = 0.1f;
		model.frameinfo[i].num_frames = 1;
		model.frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t));
		model.frameinfo[i].frames[0].name = copystring(md2frame->name);
		model.frameinfo[i].frames[0].offset = i;
	}

	model.num_meshes = 1;
	model.meshes = (mesh_t*)qmalloc(sizeof(mesh_t));

	model.num_tags = 0;
	model.tags = NULL;

	model.flags = 0;
	model.synctype = 0;
	model.offsets[0] = 0.0f;
	model.offsets[1] = 0.0f;
	model.offsets[2] = 0.0f;

	mesh = &model.meshes[0];
	mesh_initialize(&model, mesh);

	mesh->name = copystring("md2mesh");

	mesh->skins = (meshskin_t*)qmalloc(sizeof(meshskin_t) * model.num_skins);
	for (i = 0; i < model.num_skins; i++)
	{
		void *filedata;
		size_t filesize;
		char *error;
		image_rgba_t *image;

		for (j = 0; j < SKIN_NUMTYPES; j++)
			mesh->skins[i].components[j] = NULL;

	/* try to load the image file mentioned in the md2 */
	/* if any of the skins fail to load, they will be left as null */
		if (!loadfile(md2skins[i].name, &filedata, &filesize, &error))
		{
			printf("md2: failed to load file \"%s\": %s\n", md2skins[i].name, error);
			qfree(error);
			continue;
		}

		image = image_load(md2skins[i].name, filedata, filesize, &error);
		if (!image)
		{
			printf("md2: failed to load image \"%s\": %s\n", md2skins[i].name, error);
			qfree(error);
			qfree(filedata);
			continue;
		}

		qfree(filedata);

		mesh->skins[i].components[SKIN_DIFFUSE] = image;
	}

	mesh->num_triangles = header->num_tris;
	mesh->triangle3i = (int*)qmalloc(sizeof(int) * mesh->num_triangles * 3);

	mesh->num_vertices = 0;
	meshverts = (md2_meshvert_t*)qmalloc(sizeof(md2_meshvert_t) * mesh->num_triangles * 3);
	for (i = 0; i < mesh->num_triangles; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int vertnum;
			unsigned short xyz = dtriangles[i].index_xyz[j];
			unsigned short st = dtriangles[i].index_st[j];

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
		mesh->texcoord2f[i*2+0] = (dstverts[meshverts[i].texcoord].s + 0.5f) * iwidth;
		mesh->texcoord2f[i*2+1] = (dstverts[meshverts[i].texcoord].t + 0.5f) * iheight;
	}

	mesh->vertex3f = (float*)qmalloc(model.num_frames * sizeof(float) * mesh->num_vertices * 3);
	mesh->normal3f = (float*)qmalloc(model.num_frames * sizeof(float) * mesh->num_vertices * 3);
	f += header->offset_frames;
	for (i = 0; i < model.num_frames; i++)
	{
		daliasframe_t *frame = (daliasframe_t*)f;
		int firstvertex = i * mesh->num_vertices * 3;

		for (j = 0; j < mesh->num_vertices; j++)
		{
			dtrivertx_t *vtx = &frame->verts[meshverts[j].vertex];
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

	*out_model = model;
	return true;
}

static char *md2_create_skin_filename(const char *skinname)
{
	char temp[1024];
	char *c;

/* in case skinname is already a file path, strip path and extension (so "models/something/skin.pcx" becomes "skin") */
	if ((c = strrchr(skinname, '/')))
		strcpy(temp, c + 1);
	else
		strcpy(temp, skinname);

	if ((c = strchr(temp, '.')))
		*c = '\0';

	if (g_skinpath && g_skinpath[0])
		return msprintf("%s/%s.pcx", g_skinpath, temp);
	else
		return msprintf("%s.pcx", temp);
}

typedef struct md2_data_s
{
	dstvert_t *texcoords; /* [numtexcoords] */
	int numtexcoords; /* <= mesh->num_vertices */
	int *texcoord_lookup; /* [mesh->num_vertices] */

	daliasframe_t *frames; /* [model->num_frames] */

	dtrivertx_t *original_vertices; /* [mesh->num_vertices * model->num_frames] */

	int *vertices; /* [numvertices] ; index into original_vertices */
	int numvertices;
	int *vertex_lookup; /* [mesh->num_vertices] index into vertices */
} md2_data_t;

static md2_data_t *md2_process_vertices(const model_t *model, const mesh_t *mesh, int skinwidth, int skinheight)
{
	md2_data_t *data;
	int i, j, k;

	data = (md2_data_t*)qmalloc(sizeof(md2_data_t));

/* convert texcoords to integer and combine duplicates */
	data->texcoords = (dstvert_t*)qmalloc(sizeof(dstvert_t) * mesh->num_vertices);
	data->numtexcoords = 0;
	data->texcoord_lookup = (int*)qmalloc(sizeof(int) * mesh->num_vertices);

	for (i = 0; i < mesh->num_vertices; i++)
	{
		dstvert_t md2texcoord;

		md2texcoord.s = (int)(mesh->texcoord2f[i*2+0] * skinwidth);
		md2texcoord.t = (int)(mesh->texcoord2f[i*2+1] * skinheight);

		for (j = 0; j < data->numtexcoords; j++)
			if (md2texcoord.s == data->texcoords[j].s && md2texcoord.t == data->texcoords[j].t)
				break;
		if (j == data->numtexcoords)
			data->texcoords[data->numtexcoords++] = md2texcoord;
		data->texcoord_lookup[i] = j;
	}

/* compress vertices */
	data->frames = (daliasframe_t*)qmalloc(sizeof(daliasframe_t) * model->num_frames);
	data->original_vertices = (dtrivertx_t*)qmalloc(sizeof(dtrivertx_t) * mesh->num_vertices * model->num_frames);

	for (i = 0; i < model->num_frames; i++)
	{
		daliasframe_t *md2frame = &data->frames[i];
		const float *v, *n;
		float mins[3], maxs[3], iscale[3];

	/* calculate bounds of frame */
		VectorClear(mins);
		VectorClear(maxs);
		v = mesh->vertex3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		for (j = 0; j < mesh->num_vertices; j++, v += 3)
		{
			for (k = 0; k < 3; k++)
			{
				mins[k] = (j == 0) ? v[k] : min(mins[k], v[k]);
				maxs[k] = (j == 0) ? v[k] : max(maxs[k], v[k]);
			}
		}

		for (j = 0; j < 3; j++)
		{
			md2frame->scale[j] = (maxs[j] - mins[j]) * (1.0f / 255.0f);
			md2frame->translate[j] = mins[j];

			iscale[j] = md2frame->scale[j] ? (1.0f / md2frame->scale[j]) : 0.0f;
		}

	/* compress vertices */
		v = mesh->vertex3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		n = mesh->normal3f + model->frameinfo[i].frames[0].offset * mesh->num_vertices * 3;
		for (j = 0; j < mesh->num_vertices; j++, v += 3, n += 3)
		{
			dtrivertx_t *md2vertex = &data->original_vertices[j * model->num_frames + i];

			for (k = 0; k < 3; k++)
			{
				float pos = (v[k] - md2frame->translate[k]) * iscale[k];

				pos = (float)floor(pos + 0.5f);
				pos = bound(0.0f, pos, 255.0f);

				md2vertex->v[k] = (unsigned char)pos;
			}

			md2vertex->lightnormalindex = compress_normal(n);
		}
	}

/* combine duplicate vertices */
	data->vertices = (int*)qmalloc(sizeof(int) * mesh->num_vertices);
	data->numvertices = 0;
	data->vertex_lookup = (int*)qmalloc(sizeof(int) * mesh->num_vertices);

	for (i = 0; i < mesh->num_vertices; i++)
	{
		const dtrivertx_t *v1 = data->original_vertices + i * model->num_frames;

	/* see if a vertex at this position already exists */
		for (j = 0; j < data->numvertices; j++)
		{
			const dtrivertx_t *v2 = data->original_vertices + data->vertices[j] * model->num_frames;

			for (k = 0; k < model->num_frames; k++)
				if (v1[k].v[0] != v2[k].v[0] || v1[k].v[1] != v2[k].v[1] || v1[k].v[2] != v2[k].v[2] || v1[k].lightnormalindex != v2[k].lightnormalindex)
					break;
			if (k == model->num_frames)
				break;
		}
		if (j == data->numvertices)
		{
		/* no match, add this one */
			data->vertices[data->numvertices++] = i;
		}
		data->vertex_lookup[i] = j;
	}

	return data;
}

static void md2_free_data(md2_data_t *data)
{
	qfree(data->texcoords);
	qfree(data->texcoord_lookup);
	qfree(data->frames);
	qfree(data->original_vertices);
	qfree(data->vertices);
	qfree(data->vertex_lookup);
	qfree(data);
}

/* md2 glcmds generation, taken from quake2 source (models.c) */

static int commands[16384];
static int numcommands;
static int *used;

static int strip_xyz[128];
static int strip_st[128];
static int strip_tris[128];
static int stripcount;

static dtriangle_t *triangles;
static int num_tris;

static int md2_strip_length(int starttri, int startv)
{
	int m1, m2;
	int st1, st2;
	int j;
	dtriangle_t *last, *check;
	int k;

	used[starttri] = 2;

	last = &triangles[starttri];

	strip_xyz[0] = last->index_xyz[(startv  )%3];
	strip_xyz[1] = last->index_xyz[(startv+1)%3];
	strip_xyz[2] = last->index_xyz[(startv+2)%3];
	strip_st[0] = last->index_st[(startv  )%3];
	strip_st[1] = last->index_st[(startv+1)%3];
	strip_st[2] = last->index_st[(startv+2)%3];

	strip_tris[0] = starttri;
	stripcount = 1;

	m1 = last->index_xyz[(startv+2)%3];
	st1 = last->index_st[(startv+2)%3];
	m2 = last->index_xyz[(startv+1)%3];
	st2 = last->index_st[(startv+1)%3];

/* look for a matching triangle */
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < num_tris; j++, check++)
	{
		for (k = 0; k < 3; k++)
		{
			if (check->index_xyz[k] != m1)
				continue;
			if (check->index_st[k] != st1)
				continue;
			if (check->index_xyz[(k+1)%3] != m2)
				continue;
			if (check->index_st[(k+1)%3] != st2)
				continue;

		/* this is the next part of the fan */

		/* if we can't use this triangle, this tristrip is done */
			if (used[j])
				goto done;

		/* the new edge */
			if (stripcount & 1)
			{
				m2 = check->index_xyz[(k+2)%3];
				st2 = check->index_st[(k+2)%3];
			}
			else
			{
				m1 = check->index_xyz[(k+2)%3];
				st1 = check->index_st[(k+2)%3];
			}

			strip_xyz[stripcount+2] = check->index_xyz[(k+2)%3];
			strip_st[stripcount+2] = check->index_st[(k+2)%3];
			strip_tris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

/* clear the temp used flags */
	for (j = starttri + 1; j < num_tris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

static int md2_fan_length(int starttri, int startv)
{
	int m1, m2;
	int st1, st2;
	int j;
	dtriangle_t *last, *check;
	int k;

	used[starttri] = 2;

	last = &triangles[starttri];

	strip_xyz[0] = last->index_xyz[(startv  )%3];
	strip_xyz[1] = last->index_xyz[(startv+1)%3];
	strip_xyz[2] = last->index_xyz[(startv+2)%3];
	strip_st[0] = last->index_st[(startv  )%3];
	strip_st[1] = last->index_st[(startv+1)%3];
	strip_st[2] = last->index_st[(startv+2)%3];

	strip_tris[0] = starttri;
	stripcount = 1;

	m1 = last->index_xyz[(startv+0)%3];
	st1 = last->index_st[(startv+0)%3];
	m2 = last->index_xyz[(startv+2)%3];
	st2 = last->index_st[(startv+2)%3];


/* look for a matching triangle */
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < num_tris; j++, check++)
	{
		for (k = 0; k < 3; k++)
		{
			if (check->index_xyz[k] != m1)
				continue;
			if (check->index_st[k] != st1)
				continue;
			if (check->index_xyz[(k+1)%3] != m2)
				continue;
			if (check->index_st[(k+1)%3] != st2)
				continue;

		/* this is the next part of the fan */

		/* if we can't use this triangle, this tristrip is done */
			if (used[j])
				goto done;

		/* the new edge */
			m2 = check->index_xyz[(k+2)%3];
			st2 = check->index_st[(k+2)%3];

			strip_xyz[stripcount+2] = m2;
			strip_st[stripcount+2] = st2;
			strip_tris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

/* clear the temp used flags */
	for (j = starttri + 1; j < num_tris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

static void md2_build_glcmds(const dstvert_t *texcoords, int skinwidth, int skinheight)
{
	int i, j;
	int startv;
	int len, bestlen, besttype;
	int best_xyz[1024];
	int best_st[1024];
	int best_tris[1024];
	int type;

	numcommands = 0;
	used = (int*)qmalloc(sizeof(int) * num_tris);
	memset(used, 0, sizeof(int) * num_tris);
	for (i = 0; i < num_tris; i++)
	{
	/* pick an unused triangle and start the trifan */
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0; type < 2; type++)
		{
			for (startv = 0; startv < 3; startv++)
			{
				if (type == 1)
					len = md2_strip_length(i, startv);
				else
					len = md2_fan_length(i, startv);

				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j = 0; j < bestlen + 2; j++)
					{
						best_st[j] = strip_st[j];
						best_xyz[j] = strip_xyz[j];
					}
					for (j = 0; j < bestlen; j++)
						best_tris[j] = strip_tris[j];
				}
			}
		}

	/* mark the tris on the best strip/fan as used */
		for (j = 0; j < bestlen; j++)
			used[best_tris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen+2);
		else
			commands[numcommands++] = -(bestlen+2);

		for (j = 0; j < bestlen + 2; j++)
		{
			union { float f; int i; } u;
			u.f = (texcoords[best_st[j]].s + 0.5f) / skinwidth;
			commands[numcommands++] = u.i;
			u.f = (texcoords[best_st[j]].t + 0.5f) / skinheight;
			commands[numcommands++] = u.i;
			commands[numcommands++] = best_xyz[j];
		}
	}

	commands[numcommands++] = 0; /* end of list marker */

	qfree(used);
}

bool_t model_md2_save(const model_t *orig_model, void **out_data, size_t *out_size)
{
	char *error;
	unsigned char *data;
	int skinwidth, skinheight;
	char **skinfilenames;
	model_t *model;
	const mesh_t *mesh;
	md2_data_t *md2data;
	md2_header_t *header;
	int offset;
	int i, j, k;

	model = model_merge_meshes(orig_model);

	mesh = &model->meshes[0];

	skinwidth = (texwidth != -1) ? texwidth : 0;
	skinheight = (texheight != -1) ? texheight : 0;
	for (i = 0; i < model->num_skins; i++)
	{
		const skininfo_t *skininfo = &model->skininfo[i];

		for (j = 0; j < skininfo->num_skins; j++)
		{
			int offset = skininfo->skins[j].offset;

			if (!mesh->skins[offset].components[SKIN_DIFFUSE])
			{
				printf("Model has missing skin.\n");
				model_free(model);
				return false;
			}

			if (skinwidth && skinheight && (skinwidth != mesh->skins[offset].components[SKIN_DIFFUSE]->width || skinheight != mesh->skins[offset].components[SKIN_DIFFUSE]->height))
			{
				printf("Model has skins of different sizes. Use -texwidth and -texheight to resize all images to the same size.\n");
				model_free(model);
				return false;
			}
			skinwidth = mesh->skins[offset].components[SKIN_DIFFUSE]->width;
			skinheight = mesh->skins[offset].components[SKIN_DIFFUSE]->height;

		/* if fullbright texture is a different size, resample it to match the diffuse texture */
			if (mesh->skins[offset].components[SKIN_FULLBRIGHT] && (mesh->skins[offset].components[SKIN_FULLBRIGHT]->width != skinwidth || mesh->skins[offset].components[SKIN_FULLBRIGHT]->height != skinheight))
			{
				image_rgba_t *image = image_resize(mesh->skins[offset].components[SKIN_FULLBRIGHT], skinwidth, skinheight);
				image_free(&mesh->skins[offset].components[SKIN_FULLBRIGHT]);
				mesh->skins[offset].components[SKIN_FULLBRIGHT] = image;
			}
		}
	}

	if (!skinwidth || !skinheight)
	{
		printf("Model has no skin. Use -texwidth and -texheight to set the skin dimensions, or  -tex to import a skin.\n");
		model_free(model);
		return false;
	}

/* allocate memory for writing */
	data = qmalloc(16*1024*1024); /* FIXME */

/* create 8-bit textures */
	skinfilenames = (char**)qmalloc(sizeof(char*) * model->num_skins);

	for (i = 0; i < model->num_skins; i++)
	{
		image_paletted_t *pimage;
		size_t imagesize;

		offset = model->skininfo[i].skins[0].offset; /* skingroups not supported */

		pimage = image_palettize(&quake2palette, mesh->skins[offset].components[SKIN_DIFFUSE], mesh->skins[offset].components[SKIN_FULLBRIGHT]);

		imagesize = image_pcx_save(pimage, data, 16*1024*1024);
		if (!imagesize)
		{
			printf("model_md2_save: failed to create pcx\n");
			skinfilenames[i] = NULL;
			goto skinerror;
		}

		skinfilenames[i] = md2_create_skin_filename(model->skininfo[i].skins[0].name);

		if (!writefile(skinfilenames[i], data, imagesize, &error))
		{
			printf("model_md2_save: failed to write %s: %s\n", skinfilenames[i], error);
			qfree(error);
			goto skinerror;
		}

		skinwidth = pimage->width;
		skinheight = pimage->height;
		qfree(pimage);
		continue;

skinerror:
		qfree(pimage);
		for (j = 0; j <= i; j++)
			qfree(skinfilenames[j]);
		qfree(skinfilenames);
		qfree(data);
		model_free(model);
		return false;
	}

/* optimize vertices for md2 format */
	md2data = md2_process_vertices(model, mesh, skinwidth, skinheight);

/* write header */
	offset = 0;

	header = (md2_header_t*)(data + offset);

	memcpy(header->ident, "IDP2", 4);
	header->version = 8;
	header->skinwidth = skinwidth;
	header->skinheight = skinheight;
	header->framesize = sizeof(daliasframe_t) + sizeof(dtrivertx_t) * (md2data->numvertices - 1); /* subtract one because md2_frame_t has an array of one */
	header->num_skins = model->num_skins;
	header->num_vertices = md2data->numvertices;
	header->num_st = md2data->numtexcoords;
	header->num_tris = mesh->num_triangles;
	header->num_glcmds = 0; /* filled in later */
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

	for (i = 0; i < model->num_skins; i++)
	{
		md2_skin_t *md2skin = (md2_skin_t*)(data + offset);
		strlcpy(md2skin->name, skinfilenames[i], sizeof(md2skin->name));

		offset += sizeof(md2_skin_t);
	}

/* write texcoords */
	header->offset_st = offset;

	memcpy(data + offset, md2data->texcoords, sizeof(dstvert_t) * md2data->numtexcoords);

	offset += sizeof(dstvert_t) * md2data->numtexcoords;

/* write triangles */
	header->offset_tris = offset;

	for (i = 0; i < mesh->num_triangles; i++)
	{
		dtriangle_t *dtriangle = (dtriangle_t*)(data + offset);

		for (j = 0; j < 3; j++)
		{
			dtriangle->index_xyz[j] = md2data->vertex_lookup[mesh->triangle3i[i*3+j]];
			dtriangle->index_st[j] = md2data->texcoord_lookup[mesh->triangle3i[i*3+j]];
		}

		offset += sizeof(dtriangle_t);
	}

/* write frames */
	header->offset_frames = offset;

	for (i = 0; i < model->num_frames; i++)
	{
		daliasframe_t *md2frame = (daliasframe_t*)(data + offset);

		strlcpy(md2frame->name, model->frameinfo[i].frames[0].name, sizeof(md2frame->name));

		for (j = 0; j < 3; j++)
		{
			md2frame->scale[j] = md2data->frames[i].scale[j];
			md2frame->translate[j] = md2data->frames[i].translate[j];
		}

	/* write vertices */
		for (j = 0; j < md2data->numvertices; j++)
		{
			const dtrivertx_t *md2vertex = md2data->original_vertices + md2data->vertices[j] * model->num_frames + i;

			for (k = 0; k < 3; k++)
				md2frame->verts[j].v[k] = md2vertex->v[k];

			md2frame->verts[j].lightnormalindex = md2vertex->lightnormalindex;
		}

		offset += header->framesize;
	}

/* write glcmds */
	triangles = (dtriangle_t*)(data + header->offset_tris);
	num_tris = header->num_tris;

	md2_build_glcmds((dstvert_t*)(data + header->offset_st), header->skinwidth, header->skinheight);

	header->num_glcmds = numcommands;
	header->offset_glcmds = offset;

	memcpy(data + offset, commands, sizeof(int) * numcommands);

	offset += sizeof(int) * numcommands;

/* write end */
	header->offset_end = offset;

/* done */
	md2_free_data(md2data);

	for (i = 0; i < model->num_skins; i++)
		qfree(skinfilenames[i]);
	qfree(skinfilenames);

	model_free(model);

	swap_md2(data, offset);
	*out_data = data;
	*out_size = offset;
	return true;
}
