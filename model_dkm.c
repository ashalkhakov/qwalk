/*
    QShed <http://www.icculus.org/qshed>
    Copyright 2023-2024 (C) Artyom Shalkhakov

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

#include "anorms.h"
#include "global.h"
#include "model.h"
#include "palettes.h"
#include "util.h"
#include "shaders.h"

extern const float anorms[162][3];

#define IDALIASHEADER	(('D'<<24)+('M'<<16)+('K'<<8)+'D')
#define ALIAS_VERSION	1
#define ALIAS_VERSION2	ALIAS_VERSION+1

#define	MAX_TRIANGLES	4096
#define MAX_VERTS		3072
#define MAX_FRAMES		2048
#define MAX_MD2SKINS	32
#define	MAX_SKINNAME	64
#define MAX_SEQUENCES	256

typedef struct dstvert_s
{
	short	s;
	short	t;
} dstvert_t;

typedef struct dstframe_s
{
	short	index_st[3];
} dstframe_t;

typedef struct dtriangle_s
{
	short		index_surface;
	short		num_uvframes;
	short		index_xyz[3];
	dstframe_t	stframes[1];
} dtriangle_t;

typedef struct dtrivertx_s
{
	unsigned char v[3];
	unsigned char lightnormalindex;
} dtrivertx_t;

#pragma pack( push, 1 )
typedef struct
{
	unsigned int v;				// scaled byte to fit in frame mins/maxs
	unsigned char lightnormalindex;
} dtrivertx2_t;
#pragma pack( pop )

typedef struct daliasframe_s
{
	float scale[3];
	float translate[3];
	char name[16];
	dtrivertx_t	verts[1];
} daliasframe_t;

typedef struct daliasframe2_s
{
	float scale[3];
	float translate[3];
	char name[16];
	dtrivertx2_t verts[1];
} daliasframe2_t;

#define	SRF_NODRAW		    0x00000001
#define	SRF_TRANS33		    0x00000002
#define	SRF_TRANS66		    0x00000004
#define	SRF_ALPHA		    0x00000008
#define	SRF_GLOW		    0x00000010
#define	SRF_COLORBLEND	    0x00000020
#define	SRF_HARDPOINT	    0x00000040
#define	SRF_STATICSKIN	    0x00000080
#define SRF_FULLBRIGHT      0x00000100
#define SRF_ENVMAP          0x00000200  // env map texture only
#define SRF_TEXENVMAP_ADD   0x00000400  // base texture plus env map texture
#define SRF_TEXENVMAP_MULT  0x00000800  // base texture times env map texture

typedef struct
{
	char name[32];

	int flags;

	int skinindex;
	int skinwidth;
	int skinheight;

	int num_uvframes;
} dsurface_t;

typedef struct dmdl_s 
{
    
    int ident;
    int version;
    float org[3];
    int framesize;
    int num_skins;
    int num_xyz;

    int num_st;
    int num_tris;
    int num_glcmds;
    int num_frames;
    int num_surfaces;

    int ofs_skins;
    int ofs_st;
    int ofs_tris;
    int ofs_frames;
    int ofs_glcmds; 
    int ofs_surfaces;
    int ofs_end;

} dmdl_t;

bool_t model_dkm_load(void *filedata, size_t filesize, model_t *out_model, char **out_error) {
	typedef struct dkm_meshvert_s
	{
		unsigned short vertex;
		unsigned short texcoord;
	} dkm_meshvert_t;

	int					i, j, k;
	dmdl_t				*pinmodel;
	dtriangle_t			*pintri;
	dsurface_t			*pinsurface;
	int					version;
	mem_pool_t          *pool;
	model_t             model;
	skininfo_t          *skininfo;
	frameinfo_t         *frameinfo;
    char                skin_name[MAX_SKINNAME+1];
    char                original_skin_name[MAX_SKINNAME+1];
	dkm_meshvert_t      *meshverts;
	unsigned char * const f = (unsigned char*)filedata;
	float iwidth, iheight;
	float *v, *n;
    image_rgba_t **images;

	pinmodel = (dmdl_t *)filedata;

	version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION && version != ALIAS_VERSION2)
	{
		return (void)(out_error && (*out_error = msprintf("wrong version (%i should be %i)", version, ALIAS_VERSION))), false;
	}

	pool = mem_create_pool();

	// byte swap the header fields and sanity check
	for (i=0 ; i<sizeof(dmdl_t)/4 ; i++)
		((int *)filedata)[i] = LittleLong (((int *)filedata)[i]);

	model.total_skins = pinmodel->num_skins;
	model.num_skins = pinmodel->num_skins;
	model.skininfo = (skininfo_t*)mem_alloc(pool, sizeof(skininfo_t) * model.num_skins);

    images = (image_rgba_t**)mem_alloc(pool, sizeof(image_rgba_t *) * model.num_skins);

	for (i = 0, skininfo = model.skininfo; i < model.num_skins; i++, skininfo++)
	{
        image_rgba_t *image;
        char *error;

        memcpy(skin_name,
            (const char*)(f + pinmodel->ofs_skins + i * sizeof(char[MAX_SKINNAME])),
            sizeof(char[MAX_SKINNAME]));
        skin_name[MAX_SKINNAME] = '\0';

        /* try to load the image file mentioned in the md2 */
        /* if any of the skins fail to load, they will be left as null */
        image = image_load_from_file(pool, skin_name, &error);
        if (!image)
        {
        /* this is a warning. FIXME - return warnings too, don't print them here */
            printf("dkm: failed to load image \"%s\": %s\n", skin_name, error);
        }
        replace_extension(skin_name, ".bmp", ".tga");
        replace_extension(skin_name, ".wal", ".tga");
        replace_extension(skin_name, ".pcx", ".tga");
        image = image_load_from_file(pool, skin_name, &error);
        if (!image)
        {
        /* this is a warning. FIXME - return warnings too, don't print them here */
            printf("dkm: failed to load image \"%s\": %s\n", skin_name, error);
        }
        else if ( image->num_transparent_pixels > 0 )
        {
            strip_extension(skin_name, original_skin_name);
            define_shader("gen_skins.shader", original_skin_name, skin_name, NULL, 1);
        }
        images[i] = image;

		skininfo->frametime = 0.1f;
		skininfo->num_skins = 1;
		skininfo->skins = (singleskin_t*)mem_alloc(pool, sizeof(skininfo_t));
		skininfo->skins[0].name = mem_copystring(pool, skin_name);

		skininfo->skins[0].offset = i;
	}

	model.total_frames = pinmodel->num_frames;
	model.num_frames = pinmodel->num_frames;
	model.frameinfo = (frameinfo_t*)mem_alloc(pool, sizeof(frameinfo_t) * model.num_frames);

    if (version == ALIAS_VERSION)
    {
        for (i = 0, frameinfo = model.frameinfo; i < model.num_frames; i++, frameinfo++)
        {
            const daliasframe_t *aliasframe = (const daliasframe_t*)(f + pinmodel->ofs_frames + i * pinmodel->framesize);

            frameinfo->frametime = 0.1f;
            frameinfo->num_frames = 1;
            frameinfo->frames = (singleframe_t*)mem_alloc(pool, sizeof(singleframe_t));
            frameinfo->frames[0].name = mem_copystring(pool, aliasframe->name);
            frameinfo->frames[0].offset = i;
        }
    }
    else
    {
        for (i = 0, frameinfo = model.frameinfo; i < model.num_frames; i++, frameinfo++)
        {
            const daliasframe2_t *aliasframe = (const daliasframe2_t*)(f + pinmodel->ofs_frames + i * pinmodel->framesize);

            frameinfo->frametime = 0.1f;
            frameinfo->num_frames = 1;
            frameinfo->frames = (singleframe_t*)mem_alloc(pool, sizeof(singleframe_t));
            frameinfo->frames[0].name = mem_copystring(pool, aliasframe->name);
            frameinfo->frames[0].offset = i;
        }
    }

	model.num_meshes = pinmodel->num_surfaces;
	model.meshes = (mesh_t*)mem_alloc(pool, sizeof(mesh_t) * pinmodel->num_surfaces);

	model.num_tags = 0;
	model.tags = NULL;

	model.flags = 0;
	model.synctype = 0;
	model.offsets[0] = pinmodel->org[0];
	model.offsets[1] = pinmodel->org[1];
	model.offsets[2] = pinmodel->org[2];

	pintri = (dtriangle_t *) (f + pinmodel->ofs_tris);

	for (i = 0; i < pinmodel->num_tris; i++)
	{
		pintri->index_surface = LittleShort (pintri->index_surface);
		pintri->num_uvframes = LittleShort (pintri->num_uvframes);

		for (j=0 ; j<3 ; j++)
			pintri->index_xyz[j] = LittleShort (pintri->index_xyz[j]);

		for (j=0 ; j<3 ; j++)
			for (k = 0; k < pintri->num_uvframes; k++)
				pintri->stframes[k].index_st[j] = LittleShort (pintri->stframes[k].index_st[j]);

		pintri = (dtriangle_t *) ((unsigned char *) pintri + sizeof (dtriangle_t) + ((pintri->num_uvframes - 1) * sizeof (dstframe_t)));
	}

	pinsurface = (dsurface_t *) (f + pinmodel->ofs_surfaces);

    meshverts = (dkm_meshvert_t*)mem_alloc(pool, sizeof(dkm_meshvert_t) * pinmodel->num_tris * 3);

    for (i = 0; i < model.num_meshes; i++, pinsurface++)
    {
        int num_tris;

        char surface_name[32+1];
        memcpy(surface_name, pinsurface->name, sizeof(surface_name));
        surface_name[32] = '\0';

        pinsurface->flags = LittleLong(pinsurface->flags);
        pinsurface->skinindex = LittleLong(pinsurface->skinindex);
        pinsurface->skinwidth = LittleLong(pinsurface->skinwidth);
        pinsurface->skinheight = LittleLong(pinsurface->skinheight);

		mesh_t *mesh = &model.meshes[i];

        mesh_initialize(&model, mesh);

        mesh->name = mem_copystring(pool, surface_name);

    /* read skins */
        mesh->skins = (meshskin_t*)mem_alloc(pool, sizeof(meshskin_t) * model.num_skins);
        for (j = 0; j < model.num_skins; j++)
        {
            const char *skin_name;
            image_rgba_t *image;

            skininfo = &model.skininfo[j];
            skin_name = skininfo->skins[0].name;

            for (k = 0; k < SKIN_NUMTYPES; k++)
                mesh->skins[j].components[k] = NULL;

        /* try to load the image file mentioned in the dkm */
        /* if any of the skins fail to load, they will be left as null */
            image = images[j];
            if (!image)
            {
                continue;
            }

            mesh->skins[j].components[SKIN_DIFFUSE] = image_clone(mem_globalpool, image);
            mesh->skins[j].components[SKIN_FULLBRIGHT] = NULL;
        }

    /* count surface triangles */
        num_tris = 0;
        pintri = (dtriangle_t *) (f + pinmodel->ofs_tris);
        for (j = 0; j < pinmodel->num_tris; j++) {
            if (pintri->index_surface == i) {
                num_tris++;
            }
            pintri = (dtriangle_t *) ((unsigned char *) pintri + sizeof (dtriangle_t) + ((pintri->num_uvframes - 1) * sizeof (dstframe_t)));
        }

    /* read triangles */
        mesh->triangle3i = (int*)mem_alloc(pool, sizeof(int) * num_tris * 3);

        mesh->num_triangles = 0;
        mesh->num_vertices = 0;
        pintri = (dtriangle_t *) (f + pinmodel->ofs_tris);
        for (j = 0; j < pinmodel->num_tris; j++,
                                  pintri = (dtriangle_t *) ((unsigned char *) pintri + sizeof (dtriangle_t) + ((pintri->num_uvframes - 1) * sizeof (dstframe_t))))
        {
            if (pintri->index_surface != i)
            {
                continue;
            }

            for (k = 0; k < 3; k++)
            {
                int vertnum;
                unsigned short xyz = pintri->index_xyz[k];
                unsigned short st = pintri->stframes[0].index_st[k];

                for (vertnum = 0; vertnum < mesh->num_vertices; vertnum++)
                {
                /* see if this xyz+st combination exists in the buffer yet */
                    dkm_meshvert_t *mv = &meshverts[vertnum];

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

                mesh->triangle3i[mesh->num_triangles * 3 + k] = vertnum; /* (clockwise winding) */
            }
            mesh->num_triangles++;
        }

    /* read texcoords */
        mesh->texcoord2f = (float*)mem_alloc(pool, sizeof(float) * mesh->num_vertices * 2);
        iwidth = 1.0f / pinsurface->skinwidth;
        iheight = 1.0f / pinsurface->skinheight;
        for (j = 0; j < mesh->num_vertices; j++)
        {
            const dstvert_t *dstvert = (const dstvert_t*)(f + pinmodel->ofs_st) + meshverts[j].texcoord;
            mesh->texcoord2f[j*2+0] = (LittleFloat(dstvert->s) + 0.5f) * iwidth;
            mesh->texcoord2f[j*2+1] = (LittleFloat(dstvert->t) + 0.5f) * iheight;
        }

    /* read frames */
        v = mesh->vertex3f = (float*)mem_alloc(pool, model.num_frames * sizeof(float) * mesh->num_vertices * 3);
        n = mesh->normal3f = (float*)mem_alloc(pool, model.num_frames * sizeof(float) * mesh->num_vertices * 3);
	    if (version == ALIAS_VERSION)
        {
            for (j = 0; j < model.num_frames; j++)
            {
                const daliasframe_t *frame = (const daliasframe_t*)(f + pinmodel->ofs_frames + j * pinmodel->framesize);
                const dtrivertx_t *vtxbase = (const dtrivertx_t*)(frame->verts);
                float scale[3], translate[3];

                for (k = 0; k < 3; k++)
                {
                    scale[k] = LittleFloat(frame->scale[k]);
                    translate[k] = LittleFloat(frame->translate[k]);
                }

                for (k = 0; k < mesh->num_vertices; k++, v += 3, n += 3)
                {
                    const dtrivertx_t *vtx = vtxbase + meshverts[k].vertex;
                    v[0] = translate[0] + scale[0] * vtx->v[0];
                    v[1] = translate[1] + scale[1] * vtx->v[1];
                    v[2] = translate[2] + scale[2] * vtx->v[2];

                    n[0] = anorms[vtx->lightnormalindex][0];
                    n[1] = anorms[vtx->lightnormalindex][1];
                    n[2] = anorms[vtx->lightnormalindex][2];
                }
            }
        }
        else
        {
            for (j = 0; j < model.num_frames; j++)
            {
                const daliasframe2_t *frame = (const daliasframe2_t*)(f + pinmodel->ofs_frames + j * pinmodel->framesize);
                const dtrivertx2_t *vtxbase = (const dtrivertx2_t*)(frame->verts);
                float scale[3], translate[3];

                for (k = 0; k < 3; k++)
                {
                    scale[k] = LittleFloat(frame->scale[k]);
                    translate[k] = LittleFloat(frame->translate[k]);
                }

                for (k = 0; k < mesh->num_vertices; k++, v += 3, n += 3)
                {
                    const dtrivertx2_t *vtx = vtxbase + meshverts[k].vertex;
                    int encoded = LittleLong(vtx->v);

                    v[0] = translate[0] + scale[0] * ((encoded >> 21) & ((1 << 11) - 1));
                    v[1] = translate[1] + scale[1] * ((encoded >> 11) & ((1 << 10) - 1));
                    v[2] = translate[2] + scale[2] * ((encoded >>  0) & ((1 << 11) - 1));

                    n[0] = anorms[vtx->lightnormalindex][0];
                    n[1] = anorms[vtx->lightnormalindex][1];
                    n[2] = anorms[vtx->lightnormalindex][2];
                }
            }
        }
    }

    mem_free(meshverts);
    for (i = 0; i < model.num_skins; i++) {
        image_rgba_t *image;

        image = images[i];
        if (!image)
        {
            continue;
        }

	    image_free(&image);
    }
    mem_free(images);

    mem_merge_pool(pool);

    *out_model = model;
    return true;
}
