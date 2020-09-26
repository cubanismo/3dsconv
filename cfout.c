#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "internal.h"
#include "proto.h"


static void
writeheader(FILE *f, Object *obj)
{
	char *label = name2label(obj->name);

	fprintf(f, "\nstatic C3DObjdata %s_data = {\n", label);
	fprintf(f, "\t%d,\t/* Number of faces */\n", obj->numPolys);
	fprintf(f, "\t%d,\t/* Number of points */\n", obj->numVerts);
	fprintf(f, "\t%d,\t/* Number of materials */\n", numMaterials);
	fprintf(f, "\t0,\t/* reserved word */\n");
	fprintf(f, "\tfacelist%s,\n", label);
	fprintf(f, "\tvertlist%s,\n", label);
	fprintf(f, "\tmatlist\n");
	fprintf(f, "};\n\n");

	fprintf(f, "C3DObject %s = {\n", label);
	fprintf(f, "\t&%s_data,\n", label);
	fprintf(f, "\t{ 1.0, 0, 0,\n");
	fprintf(f, "\t  0, 1.0, 0,\n");
	fprintf(f, "\t  0, 0, 1.0,\n");
	fprintf(f, "\t  0, 0, 0 },\n");
	fprintf(f, "};\n");
}

/* convert a float to a signed integer */
#define TOINT(x) ((int)rint((x)))

/* convert a float to a 0.14 fixed point number: uses the "tofixed" function */
#define TOFIXED(x)  ( ((int)rint(16384.0*(x))) & 0x0000ffff)

/* convert a float to a 0.8 fixed point number */
#define TOBYTE(x) ((int)((x)*255.9))

/* default texture coordinates */
static double
default_u[] = { 0.0, 0.0, 1.0, 1.0 };

static double
default_v[] = { 0.0, 1.0, 0.0, 1.0 };

static void
writefaces(FILE *f, Object *obj)
{
	int i, j;
	double fd;
	_Polygon *p;
	double *text_u, *text_v;
	Vertex *verttab;

	fprintf(f, "static short facelist%s[] = {\n", name2label(obj->name));
	p = obj->polytab;
	verttab = obj->verttab;

	for (i = 0; i < obj->numPolys; i++,p++) {
		fprintf(f, "/* Face %d */\n", i);
		fprintf(f, "\t%d,\t\t/* number of points */\n", p->numverts);
		fprintf(f, "\t%d,\t\t/* material %s */\n", p->material, mattab[p->material].name);

		fd = p->fx * verttab[p->vert[0]].x + p->fy * verttab[p->vert[0]].y + p->fz * verttab[p->vert[0]].z;
		fprintf(f, "\t0x%x,0x%x,0x%x,0x%x,\t/* face normal */\n",
			TOFIXED(p->fx), TOFIXED(p->fy), TOFIXED(p->fz),
			TOINT(-fd) & 0x0000ffff
		);
		for (j = 0; j < p->numverts; j++) {
			fprintf(f, "\t%d, ", p->vert[j]);
			/* if texture coordinates are provided, use those */
			if (mattab[p->material].texmap) {
				text_u = p->u; text_v = p->v;
			} else {
				text_u = default_u; text_v = default_v;
			}
			fprintf(f, "0x%02x%02x,\t/* Point index, texture coordinates */\n", TOBYTE(text_u[j]), TOBYTE(text_v[j]));
		}
	}
	fprintf(f, "};\n");
}

static void
writeverts(FILE *f, Object *obj)
{
	int i;
	Vertex *verttab = obj->verttab;

	fprintf(f, "\nstatic Point vertlist%s[] = {\n", name2label(obj->name));
	for (i = 0; i < obj->numVerts; i++) {
		fprintf(f, "\t/* Vertex %d */\n", i);
		fprintf(f, "\t{%f,%f,%f,\t/* coordinates */\n",
			verttab[i].x, verttab[i].y, verttab[i].z );
		fprintf(f, "\t%f,%f,%f\t/* vertex normal */},\n",
			verttab[i].vx, verttab[i].vy, verttab[i].vz );
	}
	fprintf(f, "};\n");
}

/* flag: set to 1 when the materials are output for the first time */
static int wrotemats;

static void
writemats(FILE *f, Object *obj)
{
	int i;

	if (wrotemats != 0)
		return;
	wrotemats++;

	for (i = 0; i < numMaterials; i++) {
		if (mattab[i].texmap) {
			fprintf(f, "extern short %s[];\n", name2label(mattab[i].texmap));
		}
	}

	/* now output bitmap definitions for the textures */
	for (i = 0; i < numMaterials; i++) {
		if (mattab[i].texmap) {
			fprintf(f, "static Bitmap %s_bitmap = {\n", name2label(mattab[i].texmap));
			fprintf(f, "\t%d, %d,\n", mattab[i].twidth, mattab[i].theight);
			fprintf(f, "\t%s\n", name2label(mattab[i].texmap));
			fprintf(f, "};\n\n");
		}
	}
	fprintf(f, "\n");

	fprintf(f, "\nstatic Material matlist[] = {\n");
	for (i = 0; i < numMaterials; i++) {
		fprintf(f, "{ /* Material %d: %s */\n", i, mattab[i].name);
		fprintf(f, "\t0x%04x, 0,\n", rgb2cry( mattab[i].red, mattab[i].green, mattab[i].blue ) );
		if (mattab[i].texmap) {
			fprintf(f, "\t%s_bitmap\t/* texture */\n},\n", name2label(mattab[i].texmap));
		} else {
			fprintf(f, "\t0\t\t/* no texture */\n},\n");
		}
	}
	fprintf(f, "};\n");

}

int
CFwritefile(FILE *outf, Object *obj)
{
	writefaces(outf, obj);
	writeverts(outf, obj);
	writemats(outf, obj);
	writeheader(outf, obj);
	return 0;
}
