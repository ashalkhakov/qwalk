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

#include <stdio.h>
#include <string.h>

#include "global.h"
#include "model.h"

void mesh_initialize(model_t *model, mesh_t *mesh)
{
	memset(mesh, 0, sizeof(mesh_t));
}

void mesh_free(model_t *model, mesh_t *mesh)
{
	int i, j;

	qfree(mesh->name);
	qfree(mesh->vertex3f);
	qfree(mesh->normal3f);
	qfree(mesh->texcoord2f);
	qfree(mesh->triangle3i);

	for (i = 0; i < model->total_skins; i++)
		for (j = 0; j < SKIN_NUMTYPES; j++)
			image_free(&mesh->textures[i].components[j]);
	qfree(mesh->textures);

	mesh_freerenderdata(model, mesh);
}

void mesh_generaterenderdata(model_t *model, mesh_t *mesh)
{
	int i, j, k;
	int w, h;
	float fw, fh;

	if (mesh->renderdata.initialized)
		return;

	mesh->renderdata.textures = (struct renderdata_texture*)qmalloc(sizeof(struct renderdata_texture) * model->total_skins);

	for (i = 0; i < model->total_skins; i++)
	{
		for (j = 0; j < SKIN_NUMTYPES; j++)
		{
			const image_rgba_t *component = mesh->textures[i].components[j];

			if (component)
			{
			/* pad the skin up to a power of two */
				for (w = 1; w < component->width; w <<= 1);
				for (h = 1; h < component->height; h <<= 1);

			/* generate padded texcoords */
				mesh->renderdata.textures[i].components[j].texcoord2f = (float*)qmalloc(sizeof(float[2]) * mesh->num_vertices);

				fw = (float)component->width / (float)w;
				fh = (float)component->height / (float)h;
				for (k = 0; k < mesh->num_vertices; k++)
				{
					mesh->renderdata.textures[i].components[j].texcoord2f[k*2+0] = mesh->texcoord2f[k*2+0] * fw;
					mesh->renderdata.textures[i].components[j].texcoord2f[k*2+1] = mesh->texcoord2f[k*2+1] * fh;
				}

			/* pad the skin image */
				mesh->renderdata.textures[i].components[j].image = image_pad(component, w, h);
			}
			else
			{
				mesh->renderdata.textures[i].components[j].texcoord2f = NULL;
				mesh->renderdata.textures[i].components[j].image = NULL;
			}

			mesh->renderdata.textures[i].components[j].handle = 0;
		}
	}

	mesh->renderdata.initialized = true;
}

void mesh_freerenderdata(model_t *model, mesh_t *mesh)
{
	int i, j;

	if (!mesh->renderdata.initialized)
		return;

	for (i = 0; i < model->total_skins; i++)
	{
		for (j = 0; j < SKIN_NUMTYPES; j++)
		{
			qfree(mesh->renderdata.textures[i].components[j].texcoord2f);
			image_free(&mesh->renderdata.textures[i].components[j].image);

		/* FIXME - release the uploaded texture */
		}
	}

	qfree(mesh->renderdata.textures);

	mesh->renderdata.initialized = false;
}

void model_initialize(model_t *model)
{
	memset(model, 0, sizeof(model_t));
}

void model_free(model_t *model)
{
	int i, j;
	skininfo_t *skininfo;
	singleskin_t *singleskin;
	frameinfo_t *frameinfo;
	singleframe_t *singleframe;
	tag_t *tag;
	mesh_t *mesh;

	for (i = 0, skininfo = model->skininfo; i < model->num_skins; i++, skininfo++)
	{
		for (j = 0, singleskin = skininfo->skins; j < skininfo->num_skins; j++, singleskin++)
			qfree(singleskin->name);
		qfree(skininfo->skins);
	}
	qfree(model->skininfo);

	for (i = 0, frameinfo = model->frameinfo; i < model->num_frames; i++, frameinfo++)
	{
		for (j = 0, singleframe = frameinfo->frames; j < frameinfo->num_frames; j++, singleframe++)
			qfree(singleframe->name);
		qfree(frameinfo->frames);
	}
	qfree(model->frameinfo);

	for (i = 0, tag = model->tags; i < model->num_tags; i++, tag++)
	{
		qfree(tag->name);
		qfree(tag->matrix);
	}
	qfree(model->tags);

	for (i = 0, mesh = model->meshes; i < model->num_meshes; i++, mesh++)
		mesh_free(model, mesh);
	qfree(model->meshes);

	qfree(model);
}

bool_t model_load(const char *filename, void *filedata, size_t filesize, model_t *out_model, char **out_error)
{
/* FIXME - check the file header, not the filename extension */
	const char *ext = strrchr(filename, '.');

	if (!ext)
	{
		if (out_error)
			*out_error = msprintf("no file extension");
		return false;
	}

	if (!strcasecmp(ext, ".mdl"))
		return model_mdl_load(filedata, filesize, out_model, out_error);
	if (!strcasecmp(ext, ".md2"))
		return model_md2_load(filedata, filesize, out_model, out_error);
	if (!strcasecmp(ext, ".md3"))
		return model_md3_load(filedata, filesize, out_model, out_error);

	if (out_error)
		*out_error = msprintf("unrecognized file extension");
	return false;
}

void model_generaterenderdata(model_t *model)
{
	int i;

	for (i = 0; i < model->num_meshes; i++)
		mesh_generaterenderdata(model, &model->meshes[i]);
}

void model_freerenderdata(model_t *model)
{
	int i;

	for (i = 0; i < model->num_meshes; i++)
		mesh_freerenderdata(model, &model->meshes[i]);
}

static model_t *model_clone_except_meshes(const model_t *model)
{
	model_t *newmodel = (model_t*)qmalloc(sizeof(model_t));
	int i, j;

	model_initialize(newmodel);

	newmodel->total_skins = model->total_skins;
	newmodel->num_skins = model->num_skins;
	newmodel->skininfo = (skininfo_t*)qmalloc(sizeof(skininfo_t) * model->num_skins);
	for (i = 0; i < model->num_skins; i++)
	{
		newmodel->skininfo[i].frametime = model->skininfo[i].frametime;
		newmodel->skininfo[i].num_skins = model->skininfo[i].num_skins;
		newmodel->skininfo[i].skins = (singleskin_t*)qmalloc(sizeof(singleskin_t) * model->skininfo[i].num_skins);
		for (j = 0; j < model->skininfo[i].num_skins; j++)
		{
			newmodel->skininfo[i].skins[j].name = copystring(model->skininfo[i].skins[j].name);
			newmodel->skininfo[i].skins[j].offset = model->skininfo[i].skins[j].offset;
		}
	}

	newmodel->total_frames = model->total_frames;
	newmodel->num_frames = model->num_frames;
	newmodel->frameinfo = (frameinfo_t*)qmalloc(sizeof(frameinfo_t) * model->num_frames);
	for (i = 0; i < model->num_frames; i++)
	{
		newmodel->frameinfo[i].frametime = model->frameinfo[i].frametime;
		newmodel->frameinfo[i].num_frames = model->frameinfo[i].num_frames;
		newmodel->frameinfo[i].frames = (singleframe_t*)qmalloc(sizeof(singleframe_t) * model->frameinfo[i].num_frames);
		for (j = 0; j < model->frameinfo[i].num_frames; j++)
		{
			newmodel->frameinfo[i].frames[j].name = copystring(model->frameinfo[i].frames[j].name);
			newmodel->frameinfo[i].frames[j].offset = model->frameinfo[i].frames[j].offset;
		}
	}

	newmodel->num_meshes = 0;
	newmodel->meshes = NULL;

	newmodel->num_tags = model->num_tags;
	newmodel->tags = (tag_t*)qmalloc(sizeof(tag_t) * model->num_tags);
	for (i = 0; i < model->num_tags; i++)
	{
		newmodel->tags[i].name = copystring(model->tags[i].name);
		newmodel->tags[i].matrix = (mat4x4f_t*)qmalloc(sizeof(mat4x4f_t) * model->total_frames);
		memcpy(newmodel->tags[i].matrix, model->tags[i].matrix, sizeof(mat4x4f_t) * model->total_frames);
	}

	newmodel->flags = model->flags;
	newmodel->synctype = model->synctype;
	for (i = 0; i < 3; i++)
		newmodel->offsets[i] = model->offsets[i];

	return newmodel;
}

model_t *model_clone(const model_t *model)
{
	model_t *newmodel = model_clone_except_meshes(model);
	int i, j, k;

	newmodel->num_meshes = model->num_meshes;
	newmodel->meshes = (mesh_t*)qmalloc(sizeof(mesh_t) * model->num_meshes);
	for (i = 0; i < model->num_meshes; i++)
	{
		mesh_initialize(newmodel, &newmodel->meshes[i]);

		newmodel->meshes[i].name = copystring(model->meshes[i].name);

		newmodel->meshes[i].num_vertices = model->meshes[i].num_vertices;
		newmodel->meshes[i].num_triangles = model->meshes[i].num_triangles;

		newmodel->meshes[i].vertex3f = (float*)qmalloc(sizeof(float[3]) * model->total_frames * model->meshes[i].num_vertices);
		memcpy(newmodel->meshes[i].vertex3f, model->meshes[i].vertex3f, sizeof(float[3]) * model->total_frames * model->meshes[i].num_vertices);
		newmodel->meshes[i].normal3f = (float*)qmalloc(sizeof(float[3]) * model->total_frames * model->meshes[i].num_vertices);
		memcpy(newmodel->meshes[i].normal3f, model->meshes[i].normal3f, sizeof(float[3]) * model->total_frames * model->meshes[i].num_vertices);
		newmodel->meshes[i].texcoord2f = (float*)qmalloc(sizeof(float[2]) * model->meshes[i].num_vertices);
		memcpy(newmodel->meshes[i].texcoord2f, model->meshes[i].texcoord2f, sizeof(float[2]) * model->meshes[i].num_vertices);
		newmodel->meshes[i].triangle3i = (int*)qmalloc(sizeof(int[3]) * model->meshes[i].num_triangles);
		memcpy(newmodel->meshes[i].triangle3i, model->meshes[i].triangle3i, sizeof(int[3]) * model->meshes[i].num_triangles);

		newmodel->meshes[i].textures = (texture_t*)qmalloc(sizeof(texture_t) * model->total_skins);
		for (j = 0; j < model->total_skins; j++)
			for (k = 0; k < SKIN_NUMTYPES; k++)
				newmodel->meshes[i].textures[j].components[k] = image_clone(model->meshes[i].textures[j].components[k]);
	}

	return newmodel;
}

/* create a copy of a model that has all meshes merged into one */
/* FIXME - currently assumes that all meshes use the same texture (other cases will get very complicated) */
model_t *model_merge_meshes(const model_t *model)
{
	model_t *newmodel;
	mesh_t *newmesh;
	int i, j, k;
	int ofs_verts, ofs_tris;

	if (model->num_meshes < 1)
		return NULL;
	if (model->num_meshes == 1)
		return model_clone(model);

	newmodel = model_clone_except_meshes(model);

	newmodel->num_meshes = 1;
	newmodel->meshes = (mesh_t*)qmalloc(sizeof(mesh_t));

	newmesh = &newmodel->meshes[0];

	mesh_initialize(newmodel, newmesh);

	newmesh->name = copystring("merged");

	for (i = 0; i < model->num_meshes; i++)
	{
		newmesh->num_vertices += model->meshes[i].num_vertices;
		newmesh->num_triangles += model->meshes[i].num_triangles;
	}

	newmesh->vertex3f = (float*)qmalloc(sizeof(float[3]) * newmesh->num_vertices * newmodel->total_frames);
	newmesh->normal3f = (float*)qmalloc(sizeof(float[3]) * newmesh->num_vertices * newmodel->total_frames);
	newmesh->texcoord2f = (float*)qmalloc(sizeof(float[2]) * newmesh->num_vertices);
	newmesh->triangle3i = (int*)qmalloc(sizeof(int[3]) * newmesh->num_triangles);

	ofs_verts = 0;
	ofs_tris = 0;

	for (i = 0; i < model->num_meshes; i++)
	{
		const mesh_t *mesh = &model->meshes[i];
		float *v, *n;
		const float *iv, *in;

		for (j = 0; j < mesh->num_vertices; j++)
		{
			newmesh->texcoord2f[(ofs_verts+j)*2+0] = mesh->texcoord2f[j*2+0];
			newmesh->texcoord2f[(ofs_verts+j)*2+1] = mesh->texcoord2f[j*2+1];
		}

		iv = mesh->vertex3f;
		in = mesh->normal3f;
		for (j = 0; j < newmodel->total_frames; j++)
		{
			v = newmesh->vertex3f + j * newmesh->num_vertices * 3 + ofs_verts * 3;
			n = newmesh->normal3f + j * newmesh->num_vertices * 3 + ofs_verts * 3;

			for (k = 0; k < mesh->num_vertices; k++)
			{
				v[0] = iv[0]; v[1] = iv[1]; v[2] = iv[2]; v += 3; iv += 3;
				n[0] = in[0]; n[1] = in[1]; n[2] = in[2]; n += 3; in += 3;
			}
		}

		for (j = 0; j < mesh->num_triangles; j++)
		{
			newmesh->triangle3i[(ofs_tris+j)*3+0] = ofs_verts + mesh->triangle3i[j*3+0];
			newmesh->triangle3i[(ofs_tris+j)*3+1] = ofs_verts + mesh->triangle3i[j*3+1];
			newmesh->triangle3i[(ofs_tris+j)*3+2] = ofs_verts + mesh->triangle3i[j*3+2];
		}

		ofs_verts += mesh->num_vertices;
		ofs_tris += mesh->num_triangles;
	}

/* FIXME - this just grabs the first mesh's skin */
	newmesh->textures = (texture_t*)qmalloc(sizeof(texture_t) * newmodel->total_skins);

	for (i = 0; i < newmodel->total_skins; i++)
		for (j = 0; j < SKIN_NUMTYPES; j++)
			newmesh->textures[i].components[j] = image_clone(model->meshes[0].textures[i].components[j]);

	return newmodel;
}
