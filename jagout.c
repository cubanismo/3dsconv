#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "internal.h"
#include "proto.h"

static unsigned
mat2intcry( Material *mat )
{
	unsigned cry;

	cry = rgb2cry(mat->red, mat->green, mat->blue);
	return (cry >> 8) | ((cry << 8) & 0xff00);
}

static void
writeheader(FILE *f, Object *obj)
{
	char *label = name2label(obj->name);

	fprintf(f, ".%s_data:\n", label);
	fprintf(f, "\tdc.w\t%d,%d\t\t;Number of points, Number of faces\n",
		obj->numVerts, obj->numPolys);
	fprintf(f, "\tdc.l\t.vertlist%s\n", label);
	fprintf(f, "\tdc.l\t.texlist\n");
	fprintf(f, "\tdc.l\t.tboxlist%s\n", label);
}

/* convert a float to a signed integer */
#define TOINT(x) ((int)rint((x)))

/* convert a float to a 0.14 fixed point number: uses the "tofixed" function */
#define TOFIXED(x)  ( ((int)rint(16384.0*(x))) & 0x0000ffff)

/* convert a float to a 0.8 fixed point number */
#define TOBYTE(x) ((int)((x)*255.9))

static int tboxnum;		/* global variable; keeps track of number of tboxes emitted */

static void
writefaces(FILE *f, Object *obj)
{
	int i, j;
	_Polygon *p;
	int boxnum;

	boxnum = tboxnum;
	fprintf(f, ".facelist%s:\n", name2label(obj->name));
	p = obj->polytab;

	for (i = 0; i < obj->numPolys; i++,p++) {
		fprintf(f, ";* Face %d\n", i);
		if (mattab[p->material].texmap) {
			fprintf(f, "\tdc.w\t$%04x,$%04x\t;* texture mapped\n", p->material, boxnum);
			boxnum++;
		} else {
			fprintf(f, "\tdc.w\t$FFFF,$0000\t;* Gouraud shaded\n");
		}
		fprintf(f, "\tdc.w\t%d\t\t; number of points\n", p->numverts);
		fprintf(f, "\tdc.w\t$%04x\t\t; material %s\n", mat2intcry(&mattab[p->material]), mattab[p->material].name);
		for (j = 0; j < p->numverts; j++) {
			fprintf(f, "\tdc.w\t%d * 8\n", p->vert[j]);
		}
		fprintf(f, "\n");
	}
}

static void
writeverts(FILE *f, Object *obj)
{
	int i;

	fprintf(f, "\t.long\n");
	fprintf(f, ".vertlist%s:\n", name2label(obj->name));
	for (i = 0; i < obj->numVerts; i++) {
		fprintf(f, ";* Vertex %d\n", i);
		fprintf(f, "\tdc.w\t%d,%d,%d\t; coordinates\n",
			TOINT(obj->verttab[i].x), TOINT(obj->verttab[i].y), TOINT(obj->verttab[i].z) );
		fprintf(f, "\tdc.w\t$%04x,$%04x,$%04x\t; vertex normal\n\n",
			TOFIXED(obj->verttab[i].vx), TOFIXED(obj->verttab[i].vy), TOFIXED(obj->verttab[i].vz) );
	}
}

static int wrotetexlist = 0;

static void
writetexlist(FILE *f, Object *obj)
{
	int i;

	if (wrotetexlist)
		return;
	wrotetexlist = 1;

	for (i = 0; i < numMaterials; i++) {
		if (mattab[i].texmap) {
			fprintf(f, "\t.extern\t%s\n", name2label(mattab[i].texmap));
		}
	}

	fprintf(f, ".texlist:\n");
	for (i = 0; i < numMaterials; i++) {
		fprintf(f, "\n; Material %d: %s\n", i, mattab[i].name);
		if (mattab[i].texmap) {
			fprintf(f, "\tdc.l\t%s\t; texture\n", name2label(mattab[i].texmap));
			fprintf(f, "\tdc.l\t(PITCH1|PIXEL16|WID%d|XADDINC)\n", mattab[i].twidth);
		} else {
			fprintf(f, "\tdc.l\t0\t\t; no texture\n");
			fprintf(f, "\tdc.l\t0\n");
		}
	}
}

static void
writetboxlist(FILE *f, Object *obj)
{
	int i, j;
	int boxnum;			/* temporary copy of boxnum */
	_Polygon *P;
	double twidth, theight;

	fprintf(f, ".tboxlist%s:\n", name2label(obj->name));

	boxnum = tboxnum;
	for (i = 0; i < obj->numPolys; i++) {
		P = &obj->polytab[i];
		if ( mattab[P->material].texmap ) {
			fprintf(f, "\tdc.l\t.pts%d\n", boxnum);
			boxnum++;
		}
	}

	boxnum = tboxnum;
	for (i = 0; i < obj->numPolys; i++) {
		P = &obj->polytab[i];
		if ( mattab[P->material].texmap ) {
			twidth = (double) mattab[P->material].twidth - 1;
			theight = (double) mattab[P->material].theight - 1;

			fprintf(f, ".pts%d:\tdc.w\t", boxnum);
			for (j = 0; j < P->numverts-1; j++) {
				fprintf(f, "%d, %d, ", TOINT(P->u[j]*twidth), TOINT(P->v[j]*theight));
			}
			/* j = P->numverts-1 here */
			fprintf(f, "%d, %d\n", TOINT(P->u[j]*twidth), TOINT(P->v[j]*theight));
			boxnum++;
		}
	}

	tboxnum = boxnum;
}

int
JAGwritefile(FILE *outf, Object *obj)
{

	writeheader(outf, obj);
	writefaces(outf, obj);
	writeverts(outf, obj);
	writetexlist(outf, obj);
	writetboxlist(outf, obj);
	return 0;
}
