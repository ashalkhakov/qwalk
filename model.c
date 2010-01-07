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

bool_t model_load(const char *filename, void *filedata, size_t filesize, model_t *out_model)
{
/* FIXME - check the file header, not the filename extension */
	const char *ext = strrchr(filename, '.');

	if (!ext)
	{
		printf("%s: no file extension\n", filename);
		return false;
	}

	if (!strncasecmp(ext, ".mdl", 4))
		return model_mdl_load(filedata, filesize, out_model);
	if (!strncasecmp(ext, ".md2", 4))
		return model_md2_load(filedata, filesize, out_model);
	if (!strncasecmp(ext, ".md3", 4))
		return model_md3_load(filedata, filesize, out_model);

	printf("unrecognized file extension \"%s\"\n", ext);
	return false;
}

void mesh_free(mesh_t *mesh)
{
	qfree(mesh->name);
	qfree(mesh->vertex3f);
	qfree(mesh->normal3f);
	qfree(mesh->texcoord2f);
	qfree(mesh->paddedtexcoord2f);
	qfree(mesh->triangle3i);
	qfree(mesh->texture.pixels);
	qfree(mesh->paddedtexture.pixels);
}

void model_free(model_t *model)
{
	int i;
	frameinfo_t *frameinfo;
	tag_t *tag;
	mesh_t *mesh;

	for (i = 0, frameinfo = model->frameinfo; i < model->num_frames; i++, frameinfo++)
		qfree(frameinfo->name);
	qfree(model->frameinfo);

	for (i = 0, tag = model->tags; i < model->num_tags; i++, tag++)
	{
		qfree(tag->name);
		qfree(tag->matrix);
	}
	qfree(model->tags);

	for (i = 0, mesh = model->meshes; i < model->num_meshes; i++, mesh++)
		mesh_free(mesh);
	qfree(model->meshes);

	qfree(model);
}

/* merge all meshes into one */
/* FIXME - currently assumes that all meshes use the same texture (other cases will get very complicated) */
mesh_t *model_merge_meshes(model_t *model)
{
	mesh_t *newmesh;
	int i, j, k;
	int ofs_verts, ofs_tris;

	if (model->num_meshes < 1)
		return NULL;
	if (model->num_meshes == 1)
		return &model->meshes[0];

	newmesh = (mesh_t*)qmalloc(sizeof(mesh_t));

	newmesh->name = NULL;

	newmesh->num_vertices = 0;
	newmesh->num_triangles = 0;

	for (i = 0; i < model->num_meshes; i++)
	{
		newmesh->num_vertices += model->meshes[i].num_vertices;
		newmesh->num_triangles += model->meshes[i].num_triangles;
	}

	newmesh->vertex3f = (float*)qmalloc(sizeof(float[3]) * newmesh->num_vertices * model->num_frames);
	newmesh->normal3f = (float*)qmalloc(sizeof(float[3]) * newmesh->num_vertices * model->num_frames);
	newmesh->texcoord2f = (float*)qmalloc(sizeof(float[2]) * newmesh->num_vertices);
	newmesh->paddedtexcoord2f = (float*)qmalloc(sizeof(float[2]) * newmesh->num_vertices);
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

			/* FIXME!! */
			if (mesh->paddedtexcoord2f)
			{
				newmesh->paddedtexcoord2f[(ofs_verts+j)*2+0] = mesh->paddedtexcoord2f[j*2+0];
				newmesh->paddedtexcoord2f[(ofs_verts+j)*2+1] = mesh->paddedtexcoord2f[j*2+1];
			}
			else
			{
				newmesh->paddedtexcoord2f[(ofs_verts+j)*2+0] = 0.0f;
				newmesh->paddedtexcoord2f[(ofs_verts+j)*2+1] = 0.0f;
			}
		}

		iv = mesh->vertex3f;
		in = mesh->normal3f;
		for (j = 0; j < model->num_frames; j++)
		{
			v = newmesh->vertex3f + j * newmesh->num_vertices * 3 + ofs_verts * 3;
			n = newmesh->normal3f + j * newmesh->num_vertices * 3 + ofs_verts * 3;

			for (k = 0; k < mesh->num_vertices; k++)
			{
				v[0] = iv[0];
				v[1] = iv[1];
				v[2] = iv[2];
				v += 3;
				iv += 3;

				n[0] = in[0];
				n[1] = in[1];
				n[2] = in[2];
				n += 3;
				in += 3;
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

	image_clone(&newmesh->texture, &model->meshes[0].texture); /* FIXME */
	image_clone(&newmesh->paddedtexture, &model->meshes[0].paddedtexture);
	newmesh->texture_handle = 0;

	for (i = 0; i < model->num_meshes; i++)
		mesh_free(&model->meshes[i]);
	qfree(model->meshes);

	model->num_meshes = 1;
	model->meshes = newmesh;

	return newmesh;
}
