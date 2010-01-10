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
#include <stdio.h> /* FIXME - don't print to console in this file */
#include <string.h>

#include "global.h"
#include "model.h"

extern const float anorms[162][3];
int compress_normal(const float *normal);

/* mdl_stvert_t::onseam */
#define ALIAS_ONSEAM 0x0020

/* mdl_itriangle_t::facesfront */
/*#define */

/* mdl_header_t::synctype */
#define ST_SYNC 0
#define ST_RAND 1

typedef enum aliasframetype_e { ALIAS_SINGLE = 0, ALIAS_GROUP } aliasframetype_t;

typedef struct mdl_header_s
{
	char id[4]; /* "IDPO" */
	int version; /* 6 */

	float scale[3]; /* model scale factors */
	float origin[3]; /* model origin */
	float radius; /* bounding radius */
	float offsets[3]; /* eye position (unused) */

	int numskins;
	int skinwidth; /* width of skin texture, must be multiple of 4 */
	int skinheight; /* height of skin texture */
	int numverts;
	int numtris;
	int numframes;
	int synctype; /* 0 = synchronized, 1 = random */
	int flags; /* model flags */

	float size; /* average size of triangles */
} mdl_header_t;

typedef struct mdl_skin_s
{
	int group; /* group = 0 */
	unsigned char *skin;
} mdl_skin_t;

typedef struct mdl_skingroup_s
{
	int group; /* group = 1 */
	int nb;
	float *time;
	unsigned char *skin;
} mdl_skingroup_t;

typedef struct mdl_stvert_s
{
	int onseam;
	int s;
	int t;
} mdl_stvert_t;

typedef struct mdl_itriangle_s
{
	int facesfront;
	int vertices[3];
} mdl_itriangle_t;

typedef struct mdl_trivertx_s
{
	unsigned char packedposition[3];
	unsigned char lightnormalindex;
} mdl_trivertx_t;

typedef struct daliasframe_s
{
	mdl_trivertx_t bboxmin; /* lightnormal isn't used */
	mdl_trivertx_t bboxmax; /* lightnormal isn't used */
	char name[16];
} daliasframe_t;

typedef struct daliasgroup_s
{
	int numframes;
	mdl_trivertx_t bboxmin; /* lightnormal isn't used */
	mdl_trivertx_t bboxmax; /* lightnormal isn't used */
} daliasgroup_t;

typedef struct daliasinterval_s
{
	float interval;
} daliasinterval_t;

typedef struct mdl_meshvert_s
{
	int vertex; /* index to mdl_trivertx_t (in mdl_simpleframe_t) */
	int s, t, back;
} mdl_meshvert_t;

/* Quake palette included for convenience - John Carmack considers it to be in the public domain. */
static palette_t quakepalette = 
{
	{
		  0,  0,  0,  15, 15, 15,  31, 31, 31,  47, 47, 47,  63, 63, 63,  75, 75, 75,  91, 91, 91, 107,107,107, 123,123,123, 139,139,139, 155,155,155, 171,171,171, 187,187,187, 203,203,203, 219,219,219, 235,235,235, 
		 15, 11,  7,  23, 15, 11,  31, 23, 11,  39, 27, 15,  47, 35, 19,  55, 43, 23,  63, 47, 23,  75, 55, 27,  83, 59, 27,  91, 67, 31,  99, 75, 31, 107, 83, 31, 115, 87, 31, 123, 95, 35, 131,103, 35, 143,111, 35, 
		 11, 11, 15,  19, 19, 27,  27, 27, 39,  39, 39, 51,  47, 47, 63,  55, 55, 75,  63, 63, 87,  71, 71,103,  79, 79,115,  91, 91,127,  99, 99,139, 107,107,151, 115,115,163, 123,123,175, 131,131,187, 139,139,203, 
		  0,  0,  0,   7,  7,  0,  11, 11,  0,  19, 19,  0,  27, 27,  0,  35, 35,  0,  43, 43,  7,  47, 47,  7,  55, 55,  7,  63, 63,  7,  71, 71,  7,  75, 75, 11,  83, 83, 11,  91, 91, 11,  99, 99, 11, 107,107, 15, 
		  7,  0,  0,  15,  0,  0,  23,  0,  0,  31,  0,  0,  39,  0,  0,  47,  0,  0,  55,  0,  0,  63,  0,  0,  71,  0,  0,  79,  0,  0,  87,  0,  0,  95,  0,  0, 103,  0,  0, 111,  0,  0, 119,  0,  0, 127,  0,  0, 
		 19, 19,  0,  27, 27,  0,  35, 35,  0,  47, 43,  0,  55, 47,  0,  67, 55,  0,  75, 59,  7,  87, 67,  7,  95, 71,  7, 107, 75, 11, 119, 83, 15, 131, 87, 19, 139, 91, 19, 151, 95, 27, 163, 99, 31, 175,103, 35, 
		 35, 19,  7,  47, 23, 11,  59, 31, 15,  75, 35, 19,  87, 43, 23,  99, 47, 31, 115, 55, 35, 127, 59, 43, 143, 67, 51, 159, 79, 51, 175, 99, 47, 191,119, 47, 207,143, 43, 223,171, 39, 239,203, 31, 255,243, 27, 
		 11,  7,  0,  27, 19,  0,  43, 35, 15,  55, 43, 19,  71, 51, 27,  83, 55, 35,  99, 63, 43, 111, 71, 51, 127, 83, 63, 139, 95, 71, 155,107, 83, 167,123, 95, 183,135,107, 195,147,123, 211,163,139, 227,179,151, 
		171,139,163, 159,127,151, 147,115,135, 139,103,123, 127, 91,111, 119, 83, 99, 107, 75, 87,  95, 63, 75,  87, 55, 67,  75, 47, 55,  67, 39, 47,  55, 31, 35,  43, 23, 27,  35, 19, 19,  23, 11, 11,  15,  7,  7, 
		187,115,159, 175,107,143, 163, 95,131, 151, 87,119, 139, 79,107, 127, 75, 95, 115, 67, 83, 107, 59, 75,  95, 51, 63,  83, 43, 55,  71, 35, 43,  59, 31, 35,  47, 23, 27,  35, 19, 19,  23, 11, 11,  15,  7,  7, 
		219,195,187, 203,179,167, 191,163,155, 175,151,139, 163,135,123, 151,123,111, 135,111, 95, 123, 99, 83, 107, 87, 71,  95, 75, 59,  83, 63, 51,  67, 51, 39,  55, 43, 31,  39, 31, 23,  27, 19, 15,  15, 11,  7, 
		111,131,123, 103,123,111,  95,115,103,  87,107, 95,  79, 99, 87,  71, 91, 79,  63, 83, 71,  55, 75, 63,  47, 67, 55,  43, 59, 47,  35, 51, 39,  31, 43, 31,  23, 35, 23,  15, 27, 19,  11, 19, 11,   7, 11,  7, 
		255,243, 27, 239,223, 23, 219,203, 19, 203,183, 15, 187,167, 15, 171,151, 11, 155,131,  7, 139,115,  7, 123, 99,  7, 107, 83,  0,  91, 71,  0,  75, 55,  0,  59, 43,  0,  43, 31,  0,  27, 15,  0,  11,  7,  0, 
		  0,  0,255,  11, 11,239,  19, 19,223,  27, 27,207,  35, 35,191,  43, 43,175,  47, 47,159,  47, 47,143,  47, 47,127,  47, 47,111,  47, 47, 95,  43, 43, 79,  35, 35, 63,  27, 27, 47,  19, 19, 31,  11, 11, 15, 
		 43,  0,  0,  59,  0,  0,  75,  7,  0,  95,  7,  0, 111, 15,  0, 127, 23,  7, 147, 31,  7, 163, 39, 11, 183, 51, 15, 195, 75, 27, 207, 99, 43, 219,127, 59, 227,151, 79, 231,171, 95, 239,191,119, 247,211,139, 
		167,123, 59, 183,155, 55, 199,195, 55, 231,227, 87, 127,191,255, 171,231,255, 215,255,255, 103,  0,  0, 139,  0,  0, 179,  0,  0, 215,  0,  0, 255,  0,  0, 255,243,147, 255,247,199, 255,255,255, 159, 91, 83
	},
	{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF }
};

static void swap_mdl(void *filedata, size_t filesize)
{
	unsigned char *f = (unsigned char*)filedata;
	mdl_header_t *header;
	int i;

	header = (mdl_header_t*)f;

	header->version    = LittleLong(header->version);
	header->scale[0]   = LittleFloat(header->scale[0]);
	header->scale[1]   = LittleFloat(header->scale[1]);
	header->scale[2]   = LittleFloat(header->scale[2]);
	header->origin[0]  = LittleFloat(header->origin[0]);
	header->origin[1]  = LittleFloat(header->origin[1]);
	header->origin[2]  = LittleFloat(header->origin[2]);
	header->radius     = LittleFloat(header->radius);
	header->offsets[0] = LittleFloat(header->offsets[0]);
	header->offsets[1] = LittleFloat(header->offsets[1]);
	header->offsets[2] = LittleFloat(header->offsets[2]);
	header->numskins   = LittleLong(header->numskins);
	header->skinwidth  = LittleLong(header->skinwidth);
	header->skinheight = LittleLong(header->skinheight);
	header->numverts   = LittleLong(header->numverts);
	header->numtris    = LittleLong(header->numtris);
	header->numframes  = LittleLong(header->numframes);
	header->synctype   = LittleLong(header->synctype);
	header->flags      = LittleLong(header->flags);
	header->size       = LittleFloat(header->size);

	f += sizeof(mdl_header_t);

/* swap skins */
	for (i = 0; i < header->numskins; i++)
	{
	/*	int *group = (int*)f;

		f += sizeof(int);

		*group = LittleLong(*group);

		switch (*group)
		{
		case 0:
			f += header->skinwidth * header->skinheight;
			break;
		case 1:  FIXME - TODO  break;
		}*/
	}
}

static int mdl_counttotalframes(const unsigned char *f, int numframes, int numvertices)
{
	int i, j;
	int total_frames = 0;

	for (i = 0; i < numframes; i++)
	{
		int type = LittleLong(*(int*)f);
		int count;

		f += sizeof(int);

		if (type == ALIAS_SINGLE)
		{
			count = 1;
		}
		else
		{
			daliasgroup_t *group = (daliasgroup_t*)f;
			f += sizeof(daliasgroup_t);
			count = LittleLong(group->numframes);

			f += count * sizeof(daliasinterval_t);
		}

		for (j = 0; j < count; j++)
		{
			f += sizeof(daliasframe_t);
			f += sizeof(mdl_trivertx_t) * numvertices;
		}

		total_frames += count;
	}

	return total_frames;
}

bool_t model_mdl_load(void *filedata, size_t filesize, model_t *out_model, char **out_error)
{
	unsigned char *f = (unsigned char*)filedata;
	mdl_header_t *header;
	int i, j, k, offset;
	mdl_skin_t *skins;
	mdl_stvert_t *stverts;
	mdl_itriangle_t *itriangles;
	mdl_trivertx_t **framevertstart;
	model_t model;
	mesh_t *mesh;
	mdl_meshvert_t *meshverts;
	float iwidth, iheight;

	header = (mdl_header_t*)f;

/* validate format */
	if (memcmp(header->id, "IDPO", 4) != 0)
	{
		if (out_error)
			*out_error = msprintf("wrong format (not IDPO)");
		return false;
	}
	if (LittleLong(header->version) != 6)
	{
		if (out_error)
			*out_error = msprintf("wrong format (version not 6)");
		return false;
	}

/* byteswap header */
	swap_mdl(filedata, filesize);

	f += sizeof(mdl_header_t);

/* read skins */
	skins = (mdl_skin_t*)qmalloc(sizeof(mdl_skin_t) * header->numskins);
	for (i = 0; i < header->numskins; i++)
	{
		int group = LittleLong(*(int*)f);

		f += sizeof(int);

		if (group == 0)
		{
		/* ordinary skin */
			skins[i].group = 0;
			skins[i].skin = (unsigned char*)f;

			f += header->skinwidth * header->skinheight;
		}
		else if (group == 1)
		{
		/* skingroup */
			if (out_error)
				*out_error = msprintf("skingroups not supported");
			qfree(skins);
			return false;
		}
		else
		{
			if (out_error)
				*out_error = msprintf("bad skin type");
			qfree(skins);
			return false;
		}
	}

/* read texcoords */
	stverts = (mdl_stvert_t*)f;
	f += sizeof(mdl_stvert_t) * header->numverts;

	for (i = 0; i < header->numverts; i++)
	{
		stverts[i].onseam = LittleLong(stverts[i].onseam);
		stverts[i].s = LittleLong(stverts[i].s);
		stverts[i].t = LittleLong(stverts[i].t);
	}

/* read triangles */
	itriangles = (mdl_itriangle_t*)f;
	f += sizeof(mdl_itriangle_t) * header->numtris;

	for (i = 0; i < header->numtris; i++)
	{
		itriangles[i].facesfront = LittleLong(itriangles[i].facesfront);
		itriangles[i].vertices[0] = LittleLong(itriangles[i].vertices[0]);
		itriangles[i].vertices[1] = LittleLong(itriangles[i].vertices[1]);
		itriangles[i].vertices[2] = LittleLong(itriangles[i].vertices[2]);
	}

/* load into a real model */
	model.num_frames = header->numframes;
	model.frameinfo = (frameinfo_t*)qmalloc(sizeof(frameinfo_t) * model.num_frames);

/* count total frames */
	model.total_frames = mdl_counttotalframes(f, header->numframes, header->numverts);

/* read frames */
	offset = 0;

	framevertstart = (mdl_trivertx_t**)qmalloc(sizeof(mdl_trivertx_t*) * model.total_frames);
	for (i = 0; i < header->numframes; i++)
	{
		int type = LittleLong(*(int*)f);

		f += sizeof(int);

		if (type == ALIAS_SINGLE)
		{
			model.frameinfo[i].frametime = 0.1f;

			model.frameinfo[i].num_frames = 1;
			model.frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t));

			model.frameinfo[i].frames[0].offset = offset;
		}
		else
		{
			daliasgroup_t *group;
			daliasinterval_t *intervals;

			group = (daliasgroup_t*)f;
			f += sizeof(daliasgroup_t);
			group->numframes = LittleLong(group->numframes);

			intervals = (daliasinterval_t*)f;
			f += group->numframes * sizeof(daliasinterval_t);

		/* load into model */
			model.frameinfo[i].frametime = LittleFloat(intervals[0].interval);

			model.frameinfo[i].num_frames = group->numframes;
			model.frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t) * group->numframes);

			for (j = 0; j < group->numframes; j++)
				model.frameinfo[i].frames[j].offset = offset + j;
		}

		for (j = 0; j < model.frameinfo[i].num_frames; j++)
		{
			daliasframe_t *sframe = (daliasframe_t*)f;
			f += sizeof(daliasframe_t);

			model.frameinfo[i].frames[j].name = (char*)qmalloc(strlen(sframe->name) + 1);
			strcpy(model.frameinfo[i].frames[j].name, sframe->name);

			framevertstart[model.frameinfo[i].frames[j].offset] = (mdl_trivertx_t*)f;
			f += header->numverts * sizeof(mdl_trivertx_t);
		}

		offset += model.frameinfo[i].num_frames;
	}

	model.num_meshes = 1;
	model.meshes = (mesh_t*)qmalloc(sizeof(mesh_t));

	model.num_tags = 0;
	model.tags = NULL;

	model.flags = header->flags;
	model.synctype = header->synctype;

	mesh = &model.meshes[0];
	mesh_initialize(mesh);

	mesh->name = (char*)qmalloc(strlen("mdlmesh") + 1);
	strcpy(mesh->name, "mdlmesh");

	mesh->num_triangles = header->numtris;
	mesh->triangle3i = (int*)qmalloc(sizeof(int) * mesh->num_triangles * 3);

	mesh->num_vertices = 0;
	meshverts = (mdl_meshvert_t*)qmalloc(sizeof(mdl_meshvert_t) * mesh->num_triangles * 3);
	for (i = 0; i < mesh->num_triangles; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int vertnum;

			int xyz = itriangles[i].vertices[j];

			int s = stverts[xyz].s;
			int t = stverts[xyz].t;
			int back = stverts[xyz].onseam && !itriangles[i].facesfront;

		/* add the vertex if it doesn't exist, otherwise use the old one */
			for (vertnum = 0; vertnum < mesh->num_vertices; vertnum++)
			{
				mdl_meshvert_t *mv = &meshverts[vertnum];

				if (mv->vertex == xyz && mv->s == s && mv->t == t && mv->back == back)
					break;
			}
			if (vertnum == mesh->num_vertices)
			{
				meshverts[vertnum].vertex = xyz;
				meshverts[vertnum].s = s;
				meshverts[vertnum].t = t;
				meshverts[vertnum].back = back;
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
		float s = (float)meshverts[i].s;
		float t = (float)meshverts[i].t;

		if (meshverts[i].back)
			s += header->skinwidth >> 1;

		mesh->texcoord2f[i*2+0] = (s + 0.5f) * iwidth;
		mesh->texcoord2f[i*2+1] = (t + 0.5f) * iheight;
	}

	mesh->vertex3f = (float*)qmalloc(model.total_frames * sizeof(float) * mesh->num_vertices * 3);
	mesh->normal3f = (float*)qmalloc(model.total_frames * sizeof(float) * mesh->num_vertices * 3);
	for (i = 0; i < model.num_frames; i++)
	{
		for (j = 0; j < model.frameinfo[i].num_frames; j++)
		{
			int firstvertex = model.frameinfo[i].frames[j].offset * mesh->num_vertices * 3;
			float *v = mesh->vertex3f + firstvertex;
			float *n = mesh->normal3f + firstvertex;

			for (k = 0; k < mesh->num_vertices; k++, v += 3, n += 3)
			{
				const mdl_trivertx_t *trivertx = &framevertstart[model.frameinfo[i].frames[j].offset][meshverts[k].vertex];

				v[0] = header->origin[0] + header->scale[0] * trivertx->packedposition[0];
				v[1] = header->origin[1] + header->scale[1] * trivertx->packedposition[1];
				v[2] = header->origin[2] + header->scale[2] * trivertx->packedposition[2];

				n[0] = anorms[trivertx->lightnormalindex][0];
				n[1] = anorms[trivertx->lightnormalindex][1];
				n[2] = anorms[trivertx->lightnormalindex][2];
			}
		}
	}

	qfree(meshverts);
	qfree(framevertstart);

	mesh->texture_diffuse.width = header->skinwidth;
	mesh->texture_diffuse.height = header->skinheight;
	mesh->texture_diffuse.pixels = (unsigned char*)qmalloc(header->skinwidth * header->skinheight * 4);
	mesh->texture_fullbright.width = header->skinwidth;
	mesh->texture_fullbright.height = header->skinheight;
	mesh->texture_fullbright.pixels = (unsigned char*)qmalloc(header->skinwidth * header->skinheight * 4);
	for (i = 0; i < header->skinwidth * header->skinheight; i++)
	{
		unsigned char c = skins[0].skin[i];

		if (quakepalette.fullbright_flags[c >> 5] & (1U << (c & 31)))
		{
		/* fullbright */
			mesh->texture_diffuse.pixels[i*4+0] = 0;
			mesh->texture_diffuse.pixels[i*4+1] = 0;
			mesh->texture_diffuse.pixels[i*4+2] = 0;
			mesh->texture_diffuse.pixels[i*4+3] = 255;

			mesh->texture_fullbright.pixels[i*4+0] = quakepalette.rgb[c*3+0];
			mesh->texture_fullbright.pixels[i*4+1] = quakepalette.rgb[c*3+1];
			mesh->texture_fullbright.pixels[i*4+2] = quakepalette.rgb[c*3+2];
			mesh->texture_fullbright.pixels[i*4+3] = 255;
		}
		else
		{
		/* normal colour */
			mesh->texture_diffuse.pixels[i*4+0] = quakepalette.rgb[c*3+0];
			mesh->texture_diffuse.pixels[i*4+1] = quakepalette.rgb[c*3+1];
			mesh->texture_diffuse.pixels[i*4+2] = quakepalette.rgb[c*3+2];
			mesh->texture_diffuse.pixels[i*4+3] = 255;

			mesh->texture_fullbright.pixels[i*4+0] = 0;
			mesh->texture_fullbright.pixels[i*4+1] = 0;
			mesh->texture_fullbright.pixels[i*4+2] = 0;
			mesh->texture_fullbright.pixels[i*4+3] = 255;
		}
	}

	qfree(skins);

	*out_model = model;
	return true;
}

/* FIXME - should refuse to save a skin with a width that's not a multiple of 4, because that will kill alignment
 * on all later data. offer an easy solution (automatic?) to pad the right side with pixels. */
bool_t model_mdl_save(const model_t *model, void **out_data, size_t *out_size)
{
	void *data;
	unsigned char *f;
	const mesh_t *mesh;
	float mins[3], maxs[3], dist[3], totalsize;
	mdl_header_t *header;
	int i, j, k;
	image_paletted_t texture;

	if (model->num_meshes != 1)
	{
	/* FIXME - create a function to combine meshes so that this can be done */
		printf("model_mdl_save: model must have only one mesh\n");
		return false;
	}

	mesh = &model->meshes[0];

/* create 8-bit texture */
	image_palettize(&mesh->texture_diffuse, &mesh->texture_fullbright, &quakepalette, &texture);

/* calculate bounds */
	mins[0] = mins[1] = mins[2] = maxs[0] = maxs[1] = maxs[2] = 0.0f;
	for (i = 0; i < model->total_frames * mesh->num_vertices; i++)
	{
		const float *xyz = mesh->vertex3f + i * 3;

		for (j = 0; j < 3; j++)
		{
			mins[j] = min(mins[j], xyz[j]);
			maxs[j] = max(maxs[j], xyz[j]);
		}
	}

	for (i = 0; i < 3; i++)
		dist[i] = (fabs(mins[i]) > fabs(maxs[i])) ? mins[i] : maxs[i];

/* calculate average polygon size (used by software engines for LOD) */
	totalsize = 0.0f;
	for (i = 0; i < mesh->num_triangles; i++)
	{
		const float *v0 = mesh->vertex3f+mesh->triangle3i[i*3+0]*3;
		const float *v1 = mesh->vertex3f+mesh->triangle3i[i*3+1]*3;
		const float *v2 = mesh->vertex3f+mesh->triangle3i[i*3+2]*3;
		float vtemp1[3], vtemp2[3], normal[3];

		VectorSubtract(v0, v1, vtemp1);
		VectorSubtract(v2, v1, vtemp2);
		CrossProduct(vtemp1, vtemp2, normal);

		totalsize += (float)sqrt(DotProduct(normal, normal)) * 0.5f;
	}

/* allocate data */
	data = qmalloc(16*1024*1024); /* FIXME */
	f = (unsigned char*)data;

/* write header */
	header = (mdl_header_t*)f;

	memcpy(header->id, "IDPO", 4);
	header->version = 6;
	header->scale[0] = (maxs[0] - mins[0]) * (1.0f / 255.9f);
	header->scale[1] = (maxs[1] - mins[1]) * (1.0f / 255.9f);
	header->scale[2] = (maxs[2] - mins[2]) * (1.0f / 255.9f);
	header->origin[0] = mins[0];
	header->origin[1] = mins[1];
	header->origin[2] = mins[2];
	header->radius = (float)sqrt(dist[0]*dist[0] + dist[1]*dist[1] + dist[2]*dist[2]);
	header->offsets[0] = 0; /* FIXME */
	header->offsets[1] = 0; /* FIXME */
	header->offsets[2] = 0; /* FIXME */
	header->numskins = 1; /* FIXME */
	header->skinwidth = mesh->texture_diffuse.width;
	header->skinheight = mesh->texture_diffuse.height;
	header->numverts = mesh->num_vertices;
	header->numtris = mesh->num_triangles;
	header->numframes = model->num_frames;
	header->synctype = model->synctype;
	header->flags = model->flags;
	header->size = totalsize / mesh->num_triangles;

	f += sizeof(mdl_header_t);

/* write skins */
	{
		mdl_skin_t *skin = (mdl_skin_t*)f;

		skin->group = 0;

		f += sizeof(mdl_skin_t) - sizeof(skin->skin);

		memcpy(f, texture.pixels, header->skinwidth * header->skinheight);
		f += header->skinwidth * header->skinheight;
	}

/* write texcoords */
	for (i = 0; i < mesh->num_vertices; i++)
	{
		mdl_stvert_t *stvert = (mdl_stvert_t*)f;

		stvert->onseam = 0;
		stvert->s = (int)(mesh->texcoord2f[i*2+0] * header->skinwidth);
		stvert->t = (int)(mesh->texcoord2f[i*2+1] * header->skinheight);

		f += sizeof(mdl_stvert_t);
	}

/* write triangles */
	for (i = 0; i < mesh->num_triangles; i++)
	{
		mdl_itriangle_t *itriangle = (mdl_itriangle_t*)f;

		itriangle->facesfront = 1;
		itriangle->vertices[0] = mesh->triangle3i[i*3+0];
		itriangle->vertices[1] = mesh->triangle3i[i*3+1];
		itriangle->vertices[2] = mesh->triangle3i[i*3+2];

		f += sizeof(mdl_itriangle_t);
	}

/* write frames */
	for (i = 0; i < model->num_frames; i++)
	{
		const frameinfo_t *frameinfo = &model->frameinfo[i];
		daliasgroup_t *aliasgroup = NULL;

		if (frameinfo->num_frames > 1)
		{
		/* framegroup */
			daliasinterval_t *intervals;

			*(int*)f = ALIAS_GROUP;
			f += sizeof(int);

			aliasgroup = (daliasgroup_t*)f;

			aliasgroup->bboxmin.packedposition[0] = 0; /* these will be set later */
			aliasgroup->bboxmin.packedposition[1] = 0;
			aliasgroup->bboxmin.packedposition[2] = 0;
			aliasgroup->bboxmin.lightnormalindex = 0;
			aliasgroup->bboxmax.packedposition[0] = 0;
			aliasgroup->bboxmax.packedposition[1] = 0;
			aliasgroup->bboxmax.packedposition[2] = 0;
			aliasgroup->bboxmax.lightnormalindex = 0;
			aliasgroup->numframes = frameinfo->num_frames;

			f += sizeof(daliasgroup_t);

			intervals = (daliasinterval_t*)f;
			for (j = 0; j < frameinfo->num_frames; j++)
				intervals[j].interval = frameinfo->frametime * (j + 1);
			f += frameinfo->num_frames * sizeof(daliasinterval_t);
		}
		else
		{
			*(int*)f = ALIAS_SINGLE;
			f += sizeof(int);
		}

		for (j = 0; j < frameinfo->num_frames; j++)
		{
			daliasframe_t *simpleframe;
			mdl_trivertx_t *trivertx;
			int offset = frameinfo->frames[j].offset;
			const float *v, *n;

			simpleframe = (daliasframe_t*)f;

			simpleframe->bboxmin.packedposition[0] = 0; /* these will be set in the following loop */
			simpleframe->bboxmin.packedposition[1] = 0;
			simpleframe->bboxmin.packedposition[2] = 0;
			simpleframe->bboxmin.lightnormalindex = 0;
			simpleframe->bboxmax.packedposition[0] = 0;
			simpleframe->bboxmax.packedposition[1] = 0;
			simpleframe->bboxmax.packedposition[2] = 0;
			simpleframe->bboxmax.lightnormalindex = 0;
			strlcpy(simpleframe->name, frameinfo->frames[j].name, sizeof(simpleframe->name));

			f += sizeof(daliasframe_t);

			v = mesh->vertex3f + offset * mesh->num_vertices * 3;
			n = mesh->normal3f + offset * mesh->num_vertices * 3;
			trivertx = (mdl_trivertx_t*)f;

			for (k = 0; k < mesh->num_vertices; k++, v += 3, n += 3, trivertx++)
			{
				float pos[3];

				pos[0] = (v[0] - header->origin[0]) / header->scale[0];
				pos[1] = (v[1] - header->origin[1]) / header->scale[1];
				pos[2] = (v[2] - header->origin[2]) / header->scale[2];

				trivertx->packedposition[0] = (unsigned char)bound(0.0f, pos[0], 255.0f);
				trivertx->packedposition[1] = (unsigned char)bound(0.0f, pos[1], 255.0f);
				trivertx->packedposition[2] = (unsigned char)bound(0.0f, pos[2], 255.0f);
				trivertx->lightnormalindex = compress_normal(n);

				if (k == 0 || trivertx->packedposition[0] < simpleframe->bboxmin.packedposition[0])
					simpleframe->bboxmin.packedposition[0] = trivertx->packedposition[0];
				if (k == 0 || trivertx->packedposition[1] < simpleframe->bboxmin.packedposition[1])
					simpleframe->bboxmin.packedposition[1] = trivertx->packedposition[1];
				if (k == 0 || trivertx->packedposition[2] < simpleframe->bboxmin.packedposition[2])
					simpleframe->bboxmin.packedposition[2] = trivertx->packedposition[2];

				if (k == 0 || trivertx->packedposition[0] > simpleframe->bboxmax.packedposition[0])
					simpleframe->bboxmax.packedposition[0] = trivertx->packedposition[0];
				if (k == 0 || trivertx->packedposition[1] > simpleframe->bboxmax.packedposition[1])
					simpleframe->bboxmax.packedposition[1] = trivertx->packedposition[1];
				if (k == 0 || trivertx->packedposition[2] > simpleframe->bboxmax.packedposition[2])
					simpleframe->bboxmax.packedposition[2] = trivertx->packedposition[2];

				if (aliasgroup)
				{
					if ((j == 0 && k == 0) || trivertx->packedposition[0] < aliasgroup->bboxmin.packedposition[0])
						aliasgroup->bboxmin.packedposition[0] = trivertx->packedposition[0];
					if ((j == 0 && k == 0) || trivertx->packedposition[1] < aliasgroup->bboxmin.packedposition[1])
						aliasgroup->bboxmin.packedposition[1] = trivertx->packedposition[1];
					if ((j == 0 && k == 0) || trivertx->packedposition[2] < aliasgroup->bboxmin.packedposition[2])
						aliasgroup->bboxmin.packedposition[2] = trivertx->packedposition[2];

					if ((j == 0 && k == 0) || trivertx->packedposition[0] > aliasgroup->bboxmax.packedposition[0])
						aliasgroup->bboxmax.packedposition[0] = trivertx->packedposition[0];
					if ((j == 0 && k == 0) || trivertx->packedposition[1] > aliasgroup->bboxmax.packedposition[1])
						aliasgroup->bboxmax.packedposition[1] = trivertx->packedposition[1];
					if ((j == 0 && k == 0) || trivertx->packedposition[2] > aliasgroup->bboxmax.packedposition[2])
						aliasgroup->bboxmax.packedposition[2] = trivertx->packedposition[2];
				}
			}

			f += mesh->num_vertices * sizeof(mdl_trivertx_t);
		}
	}

/* done */
	qfree(texture.pixels);

	swap_mdl(data, f - (unsigned char*)data);
	*out_data = data;
	*out_size = f - (unsigned char*)data;

/* print some compatibility notes (FIXME - split out to a separate function so we can also analyze existing MDLs for compatibility issues) */
	printf("Compatibility notes:\n");
	i = 0;

	if (header->skinwidth & 3)
	{
		printf("This model has a skin with a width that is not a multiple of 4. It won't (or shouldn't) run in any engine, and it may even crash some computers.\n");
		i++;
	}
	if (header->skinheight > 200)
	{
		printf("This model won't run in DOSQuake because it has a skin height greater than 200. Use the -texheight parameter to resize the skin.\n");
		i++;
	}
	if (!IS_POWER_OF_TWO(header->skinwidth) || !IS_POWER_OF_TWO(header->skinheight))
	{
		printf("This model's skin is not power-of-two and will be resampled badly in GLQuake.\n"); /* FIXME - unconfirmed */
		i++;
	}
	if (header->numframes > 256)
	{
		printf("This model has more than 256 frames and won't work in the default Quake networking protocol.\n");
		i++;
	}
	if (mesh->num_vertices > 1024)
	{
		printf("This model has more than 1024 vertices and won't load in GLQuake.\n");
		i++;
	}
	if (mesh->num_triangles > 2048)
	{
		printf("This model has more than 2048 triangles and won't load in GLQuake.\n");
		i++;
	}

	if (!i)
		printf("This model should run fine in all engines.\n");

	return true;
}
