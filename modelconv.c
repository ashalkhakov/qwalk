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
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "model.h"

static char infilename[1024] = {0};
static char outfilename[1024] = {0};

static char texfilename[1024] = {0};

static int texwidth = -1;
static int texheight = -1;

static int flags = 0;
static int synctype = 0;

static model_t *model = NULL;

/*static*/ enum format { FMT_UNKNOWN = 0, FMT_MDL, FMT_MD2, FMT_TXT } /*outformat = FMT_UNKNOWN*/;

enum format identify_extension(const char *filename)
{
	const char *ext;

	if ((ext = strrchr(filename, '.')))
	{
		if (!strcasecmp(ext, ".mdl"))
			return FMT_MDL;
		if (!strcasecmp(ext, ".md2"))
			return FMT_MD2;
		if (!strcasecmp(ext, ".txt"))
			return FMT_TXT;
	}

	return FMT_UNKNOWN;
}

bool_t replacetexture(const char *filename)
{
	void *filedata;
	size_t filesize;
	char *error;
	image_rgba_t *image;
	int i, j, k;
	mesh_t *mesh;

	if (!loadfile(filename, &filedata, &filesize, &error))
	{
		fprintf(stderr, "Failed to load %s: %s.\n", filename, error);
		qfree(error);
		return false;
	}

	image = image_load(filename, filedata, filesize, &error);
	if (!image)
	{
		fprintf(stderr, "Failed to load %s: %s.\n", filename, error);
		qfree(error);
		qfree(filedata);
		return false;
	}

	qfree(filedata);

	for (i = 0, mesh = model->meshes; i < model->num_meshes; i++, mesh++)
	{
		for (j = 0; j < model->total_skins; j++)
		{
			for (k = 0; k < SKIN_NUMTYPES; k++)
				image_free(&mesh->textures[j].components[k]); /* this also sets the image to NULL */

			mesh->textures[j].components[SKIN_DIFFUSE] = image_clone(image);
		}
	}

	image_free(&image);
	return true;
}

model_t *loadmodel(const char *filename)
{
	void *filedata;
	size_t filesize;
	char *error;
	model_t *model;

	if (!loadfile(filename, &filedata, &filesize, &error))
	{
		fprintf(stderr, "Failed to load %s: %s.\n", filename, error);
		qfree(error);
		return NULL;
	}

	model = (model_t*)qmalloc(sizeof(model_t));

	if (!model_load(filename, filedata, filesize, model, &error))
	{
		fprintf(stderr, "Failed to load %s: %s.\n", filename, error);
		qfree(error);
		qfree(model);
		model = NULL;
	}

	qfree(filedata);
	return model;
}

void dump_txt(const char *filename, const model_t *model)
{
	int i, j, k;
	FILE *fp = fopen(filename, "wt");

	if (!fp)
	{
		printf("Couldn't open %s for writing.\n", filename);
		return;
	}

	fprintf(fp, "Analysis of %s\n\n", infilename);

	fprintf(fp, "total_frames = %d\n", model->total_frames);
	fprintf(fp, "num_frames = %d\n", model->num_frames);
	fprintf(fp, "num_tags = %d\n", model->num_tags);
	fprintf(fp, "num_meshes = %d\n", model->num_meshes);
	fprintf(fp, "\n");

	fprintf(fp, "frameinfos:\n");
	if (!model->num_frames)
		fprintf(fp, "(none)\n");
	for (i = 0; i < model->num_frames; i++)
	{
		const frameinfo_t *frameinfo = &model->frameinfo[i];

		if (frameinfo->num_frames == 1)
			fprintf(fp, "%d { name = \"%s\" }\n", i, frameinfo->frames[0].name);
		else
		{
			fprintf(fp, "%d {\n", i);
			fprintf(fp, "\tframetime = %f,\n", frameinfo->frametime);
			for (j = 0; j < frameinfo->num_frames; j++)
			{
				fprintf(fp, "\t{ name = \"%s\", offset = %d }\n", frameinfo->frames[j].name, frameinfo->frames[j].offset);
			}
			fprintf(fp, "}\n");
		}
	}
	fprintf(fp, "\n");

	fprintf(fp, "tags:\n");
	if (!model->num_tags)
		fprintf(fp, "(none)\n");
	for (i = 0; i < model->num_tags; i++)
	{
		const tag_t *tag = &model->tags[i];

		fprintf(fp, "%d {\n", i);
		fprintf(fp, "\tname = \"%s\"\n", tag->name);
		fprintf(fp, "\tframes = {\n");
		for (j = 0; j < model->total_frames; j++)
		{
			fprintf(fp, "\t\t%d { %f %f %f %f, %f %f %f %f, %f %f %f %f, %f %f %f %f }\n", j, tag->matrix[j].m[0][0], tag->matrix[j].m[1][0], tag->matrix[j].m[2][0], tag->matrix[j].m[3][0], tag->matrix[j].m[0][1], tag->matrix[j].m[1][1], tag->matrix[j].m[2][1], tag->matrix[j].m[3][1], tag->matrix[j].m[0][2], tag->matrix[j].m[1][2], tag->matrix[j].m[2][2], tag->matrix[j].m[3][2], tag->matrix[j].m[0][3], tag->matrix[j].m[1][3], tag->matrix[j].m[2][3], tag->matrix[j].m[3][3]);
		}
		fprintf(fp, "\t}\n");
		fprintf(fp, "}\n");
	}
	fprintf(fp, "\n");

	fprintf(fp, "meshes:\n");
	if (!model->num_meshes)
		fprintf(fp, "(none)\n");
	for (i = 0; i < model->num_meshes; i++)
	{
		const mesh_t *mesh = &model->meshes[i];

		fprintf(fp, "%d {\n", i);
		fprintf(fp, "\tname = \"%s\"\n", mesh->name);
		fprintf(fp, "\tnum_triangles = %d\n", mesh->num_triangles);
		fprintf(fp, "\tnum_vertices = %d\n", mesh->num_vertices);
		fprintf(fp, "\n");
		fprintf(fp, "\ttriangle3i = {\n");
		for (j = 0; j < mesh->num_triangles; j++)
		{
			fprintf(fp, "\t\t%d => %d %d %d\n", j, mesh->triangle3i[j*3+0], mesh->triangle3i[j*3+1], mesh->triangle3i[j*3+2]);
		}
		fprintf(fp, "\t}");
		fprintf(fp, "\n");
		fprintf(fp, "\ttexcoord2f = {\n");
		for (j = 0; j < mesh->num_vertices; j++)
		{
			fprintf(fp, "\t\t%d => %f %f\n", j, mesh->texcoord2f[j*2+0], mesh->texcoord2f[j*2+1]);
		}
		fprintf(fp, "\t}\n");
		fprintf(fp, "\n");
		fprintf(fp, "\tframes = {\n");
		for (j = 0; j < model->total_frames; j++)
		{
			const float *v = mesh->vertex3f + j * mesh->num_vertices * 3;
			const float *n = mesh->normal3f + j * mesh->num_vertices * 3;
			fprintf(fp, "\t\tframe %d {\n", j);
			for (k = 0; k < mesh->num_vertices; k++)
			{
				fprintf(fp, "\t\t\tvertex %d { v = %f %f %f , n = %f %f %f }\n", k, v[0], v[1], v[2], n[0], n[1], n[2]);
				v += 3;
				n += 3;
			}
			fprintf(fp, "\t\t}\n");
		}
		fprintf(fp, "\t}\n");
		fprintf(fp, "}\n");
	}
	fprintf(fp, "\n");

	fclose(fp);

	printf("Saved %s.\n", filename);
}

int main(int argc, char **argv)
{
	int i, j, k;

	set_atexit_final_event(dumpleaks);
	atexit(call_atexit_events);

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (!strcmp(argv[i], "-i"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-i'\n", argv[0]);
					return 0;
				}

				strcpy(infilename, argv[i]);
			}
			else if (!strcmp(argv[i], "-tex"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-tex'\n", argv[0]);
					return 0;
				}

				strcpy(texfilename, argv[i]);
			}
			else if (!strcmp(argv[i], "-texwidth"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-texwidth'\n", argv[0]);
					return 0;
				}

				texwidth = (int)atoi(argv[i]);

				if (texwidth < 1 || texwidth > 4096)
				{
					printf("%s: invalid value for option '-texwidth'\n", argv[0]);
					return 0;
				}
			}
			else if (!strcmp(argv[i], "-texheight"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-texheight'\n", argv[0]);
					return 0;
				}

				texheight = (int)atoi(argv[i]);

				if (texheight < 1 || texheight > 4096)
				{
					printf("%s: invalid value for option '-texheight'\n", argv[0]);
					return 0;
				}
			}
			else if (!strcmp(argv[i], "-flags"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-flags'\n", argv[0]);
					return 0;
				}

				flags = (int)atoi(argv[i]);
			}
			else if (!strcmp(argv[i], "-synctype"))
			{
				if (++i == argc)
				{
					printf("%s: missing argument for option '-synctype'\n", argv[0]);
					return 0;
				}

				if (!strcmp(argv[i], "sync"))
					synctype = 0;
				else if (!strcmp(argv[i], "rand"))
					synctype = 1;
				else
				{
					printf("%s: invalid value for option '-synctype' (accepted values: 'sync' (default), 'rand')\n", argv[0]);
					return 0;
				}
			}
			else
			{
				printf("%s: unrecognized option '%s'\n", argv[0], argv[i]);
				return 0;
			}
		}
		else
		{
			strcpy(outfilename, argv[i]);
		}
	}

	if (!infilename[0])
	{
		printf("No input file specified.\n");
		return 0;
	}

	model = loadmodel(infilename);
	if (model == NULL)
		return 0;

	model->flags = flags;
	model->synctype = synctype;

	if (texfilename[0])
	{
		if (!replacetexture(texfilename))
		{
			model_free(model);
			return 1;
		}
	}

	if (texwidth > 0 || texheight > 0)
	{
		mesh_t *mesh;

		for (i = 0, mesh = model->meshes; i < model->num_meshes; i++, mesh++)
		{
			for (j = 0; j < model->total_skins; j++)
			{
				for (k = 0; k < SKIN_NUMTYPES; k++)
				{
					if (mesh->textures[j].components[j])
					{
						image_rgba_t *oldimage = mesh->textures[j].components[k];
						mesh->textures[j].components[k] = image_resize(oldimage, (texwidth > 0) ? texwidth : oldimage->width, (texheight > 0) ? texheight : oldimage->height);
						image_free(&oldimage);
					}
				}
			}
		}
	}

	if (!outfilename[0])
	{
	/* TODO - print brief analysis of input file (further analysis done on option) */
		printf("No output file specified.\n");
	}
	else
	{
		void *filedata;
		size_t filesize;

		switch (identify_extension(outfilename))
		{
		default:
			printf("Unrecognized or missing file extension.\n");
			break;
		case FMT_TXT:
			dump_txt(outfilename, model);
			break;
		case FMT_MDL:
			if (model_mdl_save(model, &filedata, &filesize))
			{
				char *error;

				if (!writefile(outfilename, filedata, filesize, &error))
				{
					printf("Failed to write %s: %s.\n", outfilename, error);
					qfree(error);
				}
				else
				{
					printf("Saved %s.\n", outfilename);
				}

				qfree(filedata);
			}
			else
			{
				printf("Failed to save model as mdl.\n");
			}
			break;
		case FMT_MD2:
			if (model_md2_save(model, &filedata, &filesize))
			{
				char *error;

				if (!writefile(outfilename, filedata, filesize, &error))
				{
					printf("Failed to write %s: %s.\n", outfilename, error);
					qfree(error);
				}
				else
				{
					printf("Saved %s.\n", outfilename);
				}

				qfree(filedata);
			}
			else
			{
				printf("Failed to save model as md2.\n");
			}
			break;
		}
	}

	model_free(model);
	return 0;
}
