#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "internal.h"
#include "proto.h"
#include <stdint.h>

extern int animflag;

/*
 * function to convert RGB to CRY
 */
unsigned
rgb2cry(int red, int green, int blue)
{
	int intensity;
	unsigned int color_offset;
	unsigned int rcomp, gcomp, bcomp;
	extern unsigned char cry[];			/* table for converting rgb->cry */

	intensity = red;				/* start with red */
	if(green > intensity)
		intensity = green;
	if(blue > intensity)
		intensity = blue;			/* get highest RGB value */
	if(intensity != 0)
	{
		rcomp = (unsigned int)red * 255 / intensity;
		gcomp = (unsigned int)green * 255 / intensity;
		bcomp = (unsigned int)blue * 255 / intensity;
	}
	else
		rcomp = gcomp = bcomp = 0;		/* R, G, B, were all 0 (black) */

	color_offset = (rcomp & 0xF8) << 7;
	color_offset |= (gcomp & 0xF8) << 2;
	color_offset |= (bcomp & 0xF8) >> 3;		/* now we have offset for cry table */

	return ( ((unsigned)cry[color_offset]) << 8) | (intensity & 0x00ff);
}

static void
writeheader(FILE *f, Object *obj)
{
	char *label = name2label(obj->name);

	fprintf(f, ".%s_data:\n", label);
	fprintf(f, "\tdc.w\t%d\t\t;Number of faces\n", obj->numPolys);
	fprintf(f, "\tdc.w\t%d\t\t;Number of points\n", obj->numVerts);
	fprintf(f, "\tdc.w\t%d\t\t;Number of materials\n", numMaterials);
	fprintf(f, "\tdc.w\t0\t\t; reserved word\n");
	fprintf(f, "\tdc.l\t.facelist%s\n", label);
	fprintf(f, "\tdc.l\t.vertlist%s\n", label);
	fprintf(f, "\tdc.l\t.matlist\n");
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

	fprintf(f, "\t.phrase\n");
	fprintf(f, ".facelist%s:\n", name2label(obj->name));
	p = obj->polytab;
	verttab = obj->verttab;

	for (i = 0; i < obj->numPolys; i++,p++) {
		fprintf(f, ";* Face %d\n", i);
		fd = p->fx * verttab[p->vert[0]].x + p->fy * verttab[p->vert[0]].y + p->fz * verttab[p->vert[0]].z;
		fprintf(f, "\tdc.w\t$%x,$%x,$%x,$%x\t; face normal\n",
			TOFIXED(p->fx), TOFIXED(p->fy), TOFIXED(p->fz),
			TOINT(-fd) & 0x0000ffff
		);
		fprintf(f, "\tdc.w\t%d\t\t; number of points\n", p->numverts);
		fprintf(f, "\tdc.w\t%d\t\t; material %s\n", p->material, mattab[p->material].name);
		for (j = 0; j < p->numverts; j++) {
			fprintf(f, "\tdc.w\t%d, ", p->vert[j]);
			/* if texture coordinates are provided, use those */
			if (mattab[p->material].texmap) {
				text_u = p->u; text_v = p->v;
			} else {
				text_u = default_u; text_v = default_v;
			}
			fprintf(f, "$%02x%02x\t; Point index, texture coordinates\n", TOBYTE(text_u[j]), TOBYTE(text_v[j]));
		}
		fprintf(f, "\n");
	}
}

static void
writeverts(FILE *f, Object *obj)
{
	int i;
	Vertex *verttab = obj->verttab;

	fprintf(f, "\t.long\n");
	fprintf(f, ".vertlist%s:\n", name2label(obj->name));
	for (i = 0; i < obj->numVerts; i++) {
		fprintf(f, ";* Vertex %d\n", i);
		fprintf(f, "\tdc.w\t%d,%d,%d\t; coordinates\n",
			TOINT(verttab[i].x), TOINT(verttab[i].y), TOINT(verttab[i].z) );
		fprintf(f, "\tdc.w\t$%04x,$%04x,$%04x\t; vertex normal\n\n",
			TOFIXED(verttab[i].vx), TOFIXED(verttab[i].vy), TOFIXED(verttab[i].vz) );
	}
	fprintf(f, "\n");
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
			fprintf(f, "\t.extern\t%s\n", name2label(mattab[i].texmap));
		}
	}

	fprintf(f, "\t.phrase\n");
	fprintf(f, ".matlist:\n");
	for (i = 0; i < numMaterials; i++) {
		fprintf(f, "\n; Material %d: %s\n", i, mattab[i].name);
		fprintf(f, "\tdc.w\t$%04x, 0\n", rgb2cry( mattab[i].red, mattab[i].green, mattab[i].blue ) );
		if (mattab[i].texmap) {
			fprintf(f, "\tdc.l\t.%s_bitmap\t; texture\n", name2label(mattab[i].texmap));
		} else {
			fprintf(f, "\tdc.l\t0\t\t; no texture\n");
		}
	}
	fprintf(f, "\n");

	/* now output bitmap definitions for the textures */
	for (i = 0; i < numMaterials; i++) {
		if (mattab[i].texmap) {
			fprintf(f, ".%s_bitmap:\n", name2label(mattab[i].texmap));
			fprintf(f, "\t.dc.w\t%d, %d\n", mattab[i].twidth, mattab[i].theight);
			fprintf(f, "\t.dc.l\tPITCH1|PIXEL16|WID%d\n", mattab[i].twidth);
			fprintf(f, "\t.dc.l\t%s\n", name2label(mattab[i].texmap));
		}
	}
	fprintf(f, "\n");
}

static void
writeanims(FILE *f, Object *obj)
{
	Matrix *M;
	int i;
	int32_t x, y, z;

	fprintf(f, ".%s_anim:\n", name2label(obj->name));
	fprintf(f, "\t.dc.w\t1, 0\t; frame animation\n");
	fprintf(f, "\t.dc.w\t%d\t; number of frames\n", obj->numframes);
	fprintf(f, "\t.dc.w\t$0002\t; frames per 300th of a second\n");
	fprintf(f, "\t.dc.l\t0\t; current frame number\n");
	for (i = 0; i < obj->numframes; i++) {
		M = &obj->frames[i];
		fprintf(f, "\t;* frame %d\n", i);
		x = TOFIXED(M->xrite); y = TOFIXED(M->yrite); z = TOFIXED(M->zrite);
		fprintf(f, "\t.dc.w\t$%04x, $%04x, $%04x\n", x, y, z);
		x = TOFIXED(M->xdown); y = TOFIXED(M->ydown); z = TOFIXED(M->zdown);
		fprintf(f, "\t.dc.w\t$%04x, $%04x, $%04x\n", x, y, z);
		x = TOFIXED(M->xhead); y = TOFIXED(M->yhead); z = TOFIXED(M->zhead);
		fprintf(f, "\t.dc.w\t$%04x, $%04x, $%04x\n", x, y, z);
		x = TOINT(M->xposn); y = TOINT(M->yposn); z = TOINT(M->zposn);
		fprintf(f, "\t.dc.w\t$%04x, $%04x, $%04x\n",
			x & 0x0000ffff, y & 0x0000ffff, z & 0x0000ffff);
	}
}

int
N3Dwritefile(FILE *outf, Object *obj)
{
	writeheader(outf, obj);
	writefaces(outf, obj);
	writeverts(outf, obj);
	writemats(outf, obj);
	if (animflag)
		writeanims(outf, obj);
	return 0;
}
