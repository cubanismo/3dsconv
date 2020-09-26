/*
 * Functions for reading a LightWave object file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifdef __DUMB_MSDOS__
#include <alloc.h>
#define mymalloc farmalloc
#define myfree farfree
#else
#define mymalloc malloc
#define myfree free
#endif

#include "internal.h"
#include "proto.h"

/* External variables */
extern double	uscale;			/* user specified scale factor */
extern char	*progname;		/* name of this program */
extern char	*infilename;		/* input file name */
extern int	verbose;		/* -v flag specified? */
extern int	multiobject;		/* output multiple objects? */

/* Global variables */
double	scale;				/* scale factor from 3DS file */

typedef struct color {
	double red, green, blue;
} COLOR;

/* local functions */
static int buildsurfinfo(uint8_t *, uint8_t *);
static float getfloat(uint8_t *);
static uint16_t getshort(void *);
static uint32_t getlong(void *);
static uint8_t *getchunk(uint8_t *, uint8_t *, char *, long *);
static uint8_t *getsubchunk(uint8_t *, uint8_t *, char *, long *);
static uint8_t *get3dpoint(uint8_t *, double *, double *, double *);

#ifdef _WIN32
#define strdup _strdup
#endif

/*
 *	read lightwave file into internal format
 *	returns: 0 on success, otherwise -1
 */

int
readlwfile(fname)
	char *fname;	/* ptr to file name */
{
	FILE *fp;
	long length;
	uint8_t *fbuf, *fbufend;	/* pointers to start and end of FORM chunk */
	long fbufsize;
	uint8_t *mdata, *mdataend;	/* pointers to start and end of other chunks */
	int i, numpolys;
	Vertex vert;
	_Polygon poly;
	Object *curobj;

	if ((fp = fopen(fname, "rb")) == NULL) {
		perror(fname);
		exit(-1);
	}
	fseek(fp, 0L, 2);	/* seek to end of file */
	fbufsize = ftell(fp);
	fseek(fp, 0L, 0);	/* rewind */

	if ((fbuf = (uint8_t *) mymalloc(fbufsize)) == NULL) {	/* get buffer */
		fprintf(stderr, "%s: insufficient memory for loading Lightwave file\n",
			progname);
		return -1;
	}
	if (fread(fbuf, fbufsize, 1, fp) != 1) {	/* read lightwave file */
		perror(fname);
		return -1;
	}
	fclose(fp);

	/*
	 * compute end of buffer 
	 */
	fbufend = fbuf + fbufsize;

	/*
	 * check that it's a FORM LWOB IFF file
	 */
	if (strncmp((char *)fbuf, "FORM", 4) != 0) {
		fprintf(stderr,"%s: %s is not a valid LWOB file.\n", progname, fname);
		return -1;
	}
	fbuf += 4;
	length = getlong(fbuf);
	fbuf += 4;
	fbufend = fbuf + length;

	if (strncmp((char *)fbuf, "LWOB", 4) != 0) {
		fprintf(stderr,"%s: %s is not a valid LWOB file.\n", progname, fname);
		return -1;
	}
	fbuf += 4;

	scale = 1.0;
	scale /= uscale;

	/*
	 * now process the various parts of the LWOB file
	 */

	/* first, get all the surface information */
	if (buildsurfinfo(fbuf, fbufend) != 0) {
		return -1;
	}

	/* next, get the point data */
	mdata = getchunk(fbuf, fbufend, "PNTS", &length);
	if (!mdata) {
		fprintf(stderr, "PNTS chunk not found in %s\n", fname);
		return -1;
	}
	mdataend = mdata + length;

	if (verbose)
		fprintf(stdout, "Getting data for %ld points\n", length/12);

	curobj = CreateObject( "Default" );

	while (mdata < mdataend) {
		vert.vx = vert.vy = vert.vz = 0;
		vert.u = vert.v = 0;
		mdata = get3dpoint(mdata, &vert.x, &vert.y, &vert.z);
		AddVertex(curobj, &vert);
	}

	/* finally, get the face records */
	mdata = getchunk(fbuf, fbufend, "POLS", &length);
	if (!mdata) {
		fprintf(stderr, "POLS chunk not found in %s\n", fname);
		return -1;
	}
	mdataend = mdata + length;
	numpolys = 0;
 	while (mdata < mdataend) {
		int curpolynum;
		int material;

		curpolynum = numpolys;
		/* polygon format: number of vertices, vertex list, material number */
		poly.numverts = getshort(mdata); mdata += 2;
		if (poly.numverts < MAXVERTICES) {
			for (i = 0; i < poly.numverts; i++) {
				poly.vert[i] = getshort(mdata); mdata += 2;
				poly.u[i] = poly.v[i] = 0.0;
			}
			CalcFaceNormal(curobj, &poly);
			AddPolygon(curobj, &poly);
			numpolys++;
		} else {
			int basevert;
			int leftside, rightside;
			int numverts;

			fprintf(stderr, "Warning: polygon has too many vertices (%d); splitting into triangles\n",
				poly.numverts);

			basevert = getshort(mdata); mdata += 2;
			rightside = getshort(mdata); mdata += 2;
			numverts = poly.numverts - 2;
			while (numverts-- > 0) {
				leftside = getshort(mdata); mdata += 2;
				poly.numverts = 3;
				poly.vert[0] = basevert;
				poly.vert[1] = rightside;
				poly.vert[2] = leftside;
				poly.u[0] = poly.v[0] = 0.0;
				poly.u[1] = poly.v[1] = 0.0;
				poly.u[2] = poly.v[2] = 0.0;
				CalcFaceNormal(curobj, &poly);
				AddPolygon(curobj, &poly);
				numpolys++;
				rightside = leftside;
			}
		}

		/* get the material number */
		material = getshort(mdata); mdata += 2;

		/* a negative material number signals the presence of detail polygons */
		if ( material < 0 ) {
			material = -material;
			mdata += 2;	/* skip the number of detail polygons */
		}

		material -= 1;

		/* make sure material number is in range */
		if ( material >= numMaterials ) {
			fprintf(stderr, "Warning: request for material %d, but only %d materials exit\n",
				material+1, numMaterials);
			material = 0;
		}
		/* change material for all polygons we added */
		for (i = curpolynum; i < numpolys; i++) {
			curobj->polytab[i].material = material;
		}
	}
	if (verbose) {
		fprintf(stdout, "%d polygons found\n", numpolys);
	}
	return 0;
}

/* 
 *	build the materials records
 */
static int
buildsurfinfo(fbuf, fbufend)
	uint8_t *fbuf;	/* start of form chunk */
	uint8_t *fbufend;	/* ...and its end */
{
	uint8_t *mdata, *mdataend;      /* start and end of SURF chunk */
	uint8_t *scstart;               /* start and end of subchunks */
	long length;
	Material mat;
	Material *oldmat;

	/* first, get the list of all materials */
	mdata = getchunk(fbuf, fbufend, "SRFS", &length);
	if (mdata == 0) {
	/* no surfaces; odd, but we can let everything be defaulted */
		return 0;
	}

	/* now create dummy materials for all surfaces */
	mdataend = mdata + length;
	mat.red = mat.green = mat.blue = 0x80;
	mat.texmap = (char *)0;
	mat.twidth = mat.theight = 64;

	while (mdata < mdataend) {
		length = 0;
		mat.name = strdup((char *)mdata);
printf("Creating material %s\n", mat.name);
		AddMaterial(&mat);
		/* skip this material's name */
		/* note that it is padded to be even length */
		do {
			length++;
		} while (*mdata++);
		if (length & 1) {	/* odd length? */
			mdata++;
		}
	}

	/* now get all the SURF chunks describing surfaces */
	for(;;) {
		mdata = getchunk(fbuf, fbufend, "SURF", &length);
		if (!mdata) break;
printf("Getting material data for %s\n", mdata);
		fbuf = mdataend = mdata + length;
		oldmat = &mattab[GetMaterial((char *)mdata)];
		if (!oldmat)
			return -1;	/* material not found? (probably can't happen) */
		/* skip over the name */
		length = 0;
		do {
			length++;
		} while (*mdata++);
		if (length & 1)
			mdata++;

		/* get color information */
		scstart = getsubchunk(mdata, mdataend, "COLR", &length);
		if (scstart) {
			oldmat->red = ((unsigned char *)scstart)[0];
			oldmat->green = ((unsigned char *)scstart)[1];
			oldmat->blue = ((unsigned char *)scstart)[2];
		}
	}
	return 0;
}


/*
 *	get a 32 bit float from a long pointer (byte swapped)
 */
static	float
getfloat(p)
	uint8_t *p;
{
	float *flp;

	*((uint32_t *)p) = getlong((char *)p);	/* looks weird, but we don't want */
	flp = (float *) p;			/* the compiler to tranfer the     */
						/* float to a uint32_t                 */
	return *flp;
}


/*	
 *	word & long swap
 */
static uint16_t
getshort(void *p0)
{
	unsigned char *p = p0;

	return (((unsigned short)p[0] << 8) | (p[1]));
}

static uint32_t
getlong(void *p0)
{
	unsigned char *p = p0;
	uint32_t d;

	d = (((uint32_t)p[0] << 24)|((uint32_t)p[1] << 16)|((uint16_t)p[2] << 8) | p[3]);

	return d;
}

/*
 *	get a specific chunk
 */

static uint8_t
*getchunk(fbp, fbend, id, length)
	uint8_t *fbp, *fbend;		/* start & end of buffer */
	char *id;			     	/* 4 byte chunk id */
	long * length;		     		/* return size of chunk in here */
{
	int32_t chunklen = 0L;
	char *chunkid;

#ifdef DEBUGCHUNK
fprintf(stderr, "getchunk: looking for (%04s)\n", id);
#endif

	while (fbp < fbend) {
		chunkid = (char *)fbp;
#ifdef DEBUGCHUNK
fprintf(stderr, "\tlooked at %04s\n", chunkid);
#endif
		fbp += 4;
		chunklen = getlong(fbp);
		fbp += 4;
		if (!strncmp(chunkid,id,4)) {
			*length = chunklen;
			return fbp;
		}
		fbp += chunklen;
	}
	return NULL;
}

/* Get a subchunk of a larger chunk.
 * Subchunks are the same as chunks, except they
 * only have a 2 byte length instead of 4 bytes
 */

static uint8_t
*getsubchunk(fbp, fbend, id, length)
	uint8_t *fbp, *fbend;		/* start & end of buffer */
	char *id;			     	/* 4 byte chunk id */
	long * length;		     		/* return size of chunk in here */
{
	int32_t chunklen = 0;
	char *chunkid;

#ifdef DEBUGCHUNK
fprintf(stderr, "getchunk: looking for (%04s)\n", id);
#endif

	while (fbp < fbend) {
		chunkid = (char *)fbp;
#ifdef DEBUGCHUNK
fprintf(stderr, "\tlooked at %04s\n", chunkid);
#endif
		fbp += 4;
		chunklen = getshort(fbp);
		fbp += 2;
		if (!strncmp(chunkid,id,4)) {
			*length = chunklen;
			return fbp;
		}
		fbp += chunklen;
	}
	return NULL;
}


static uint8_t
*get3dpoint(p, x, y, z)
	uint8_t *p;		/* data stream pointer */
	double *x, *y, *z;
{
	*x =  (getfloat(p) / scale);
	p += 4L;
	*y =  (getfloat(p) / scale);
	p += 4L;
	*z =  (getfloat(p) / scale );
	p += 4L;

	return p;
}

