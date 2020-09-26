/*
 * 3D Studio converter module for 3DSCONV.
 *
 * Copyright 1995 Atari Corporation. All Rights Reserved.
 *
 * NOTE NOTE NOTE: This file is derived from information
 * and code provided by Autodesk, Inc., and may not
 * be redistributed without their permission!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef __DUMB_MSDOS__
#include <alloc.h>
#define mymalloc farmalloc
#define myfree farfree
#else
#define mymalloc malloc
#define myfree free
#endif

#include "3dstudio.h"
#include "internal.h"
#include "proto.h"

/*#define DEBUG_KF*/

/* External variables */
extern double	uscale;			/* user specified scale factor */
extern char	*progname;		/* name of this program */
extern char	*infilename;		/* input file name */
extern int	verbose;		/* -v flag specified? */
extern int	multiobject;		/* output multiple objects? */
extern int	animflag;		/* get animation data? */

/* Global variables */
uint8_t	*fbuf;				/* buffer for loading 3ds file, */
					/* will be allocated */
uint8_t	*fbufend;			/* pts to the end of the buffer */
long	fbufsize;			/* size of the buffer */

uint8_t	*mdata;				/* start of mesh data section */
uint8_t	*mdataend;			/* end of mdata */
uint8_t	*kfdata;			/* start of key frame data section */
uint8_t	*kfdataend;

double	scale;				/* scale factor from 3DS file */


typedef struct color {
	double red, green, blue;
} COLOR;

/*
 * transformation matrices from 3Dstudio coordinate system to
 * ours, and vice versa
 */
static const Matrix Coord3DS = {
	1.0, 0.0, 0.0,
	0.0, 0.0, 1.0,
	0.0, -1.0, 0.0,
	0.0, 0.0, 0.0
};

static const Matrix Coord3DSInv = {
	1.0, 0.0, 0.0,
	0.0, 0.0, -1.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 0.0
};

static const Matrix Identity = {
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	0.0, 0.0, 0.0
};

/* local functions */
static void buildmatrecs(uint8_t *, uint8_t *);
static COLOR *get3dscolor(uint8_t *p);
static float getfloat(uint8_t *);
static uint16_t getshort(void *);
static uint32_t getlong(void *);
static int buildfacerecs(uint8_t *, uint8_t *);
static uint8_t *getntriobj(uint8_t *, uint8_t *, long *, Object **);
static uint8_t *getchunk(uint8_t *, uint8_t *, unsigned, long *);
static uint8_t *getchunkfromset(uint8_t*, uint8_t *, unsigned *, long *, unsigned *);
static uint8_t *get3dpoint(uint8_t *, double *, double *, double *);
static int buildkfdata(uint8_t *, uint8_t *);

#ifdef _WIN32
#define strdup _strdup
#endif

/*
 *	read 3d studio file into internal format
 *	returns: 0 on success, otherwise -1
 */

int
read3dsfile(fname)
	char *fname;	/* ptr to 3ds file name */
{
	FILE *fp;
	unsigned short version;
	long length;
	uint8_t *p;

	if ((fp = fopen(fname, "rb")) == NULL) {
		perror(fname);
		exit(-1);
	}
	fseek(fp, 0L, 2);	/* seek to end of file */
	fbufsize = ftell(fp);
	fseek(fp, 0L, 0);	/* rewind */

	if ((fbuf = (uint8_t *) mymalloc(fbufsize)) == NULL) {	/* get buffer */
		fprintf(stderr, "%s: insufficient memory for loading 3DS file\n",
			progname);
		return -1;
	}
	if (fread(fbuf, fbufsize, 1, fp) != 1) {	/* read 3ds file */
		perror(fname);
		return -1;
	}
	fclose(fp);

	/*
	 * compute end of buffer 
	 */
	fbufend = fbuf + fbufsize;
	
	version = getshort(fbuf);
	fbuf += 6L;			/* who needs the header now? */
	/*
	 * check for magic number 
	 */
	if (version == 0xc23dU) {
		/* a .PRJ file: that's fine */
		;
	} else if (version != 0x4d4d) {
		fprintf(stderr,"%s: %s is not a valid 3DS file.\n", progname, fname);
		return -1;
	}


	/*
	 * now process the various parts of the 3ds file
	 */
	if ((mdata = getchunk(fbuf, fbufend, MDATA, &length)) == NULL) {
		fprintf(stderr, "%s: %s has no MDATA chunk.\n", progname, fname);
		return -1;
	}
	mdataend = mdata + length;
	if ((p = getchunk(mdata, fbufend, MSCALE, &length)) == NULL) {
		fprintf(stderr, "%s: %s has no scale.\n", progname, fname);
		return -1;
	}
	scale = getfloat(p);
	if (verbose) {
		printf("Scale factor for 3DS file: %f\n", scale);
	}
	scale /= uscale;

	buildmatrecs(mdata,mdataend);

	if (buildfacerecs(mdata,mdataend) != 0)
		return (-1);

	if (animflag) {
		kfdata = getchunk(fbuf, fbufend, KFDATA, &length);
		if (kfdata == NULL) {
			fprintf(stderr, "Warning: no animation data in file\n");
			return 0;
		}
		kfdataend = kfdata + length;

		if (buildkfdata(kfdata, kfdataend) != 0)
			return -1;
	}
	return 0;
}

/* 
 *	build the materials records
 */
static void
buildmatrecs(mstart, mend)
	uint8_t *mstart;	/* start of mdata section */
	uint8_t *mend;	/* ...and its end */
{
	long length;
	uint8_t *mat, *matend;
	char *matname;			/* material name */
	uint8_t *color;			/* color chunk */
	COLOR *cptr;
	Material matrec;

	uint8_t *texmap, *texmapend;
	char *texmapname;

	if (verbose)
		fprintf(stdout, "Building materials records\n");

	for(;;) {
		if ((mat = getchunk(mstart, mend, MAT_ENTRY, &length)) == NULL) {
			break;
		}
	 	mstart = matend = mat + length;
		if ((matname = (char *)getchunk(mat, matend, MAT_NAME, &length)) == NULL) {
			break;
		}	
		if ((color = getchunk(mat, matend, MAT_DIFFUSE, &length)) == NULL) {
			break;
		}
		cptr = get3dscolor(color);
		matrec.red = 255.9*cptr->red;
		matrec.green = 255.9*cptr->green;
		matrec.blue = 255.9*cptr->blue;
		matrec.name = strdup(matname);

		/* check for a texture map */
		texmap = getchunk(mat, matend, MAT_TEXMAP, &length);
		if (texmap) {
			texmapend = texmap+length;
			/* get texture file name */
			texmapname = (char *)getchunk(texmap, texmapend, MAT_MAPNAME, &length);
		} else {
			texmapname = (char *)0;
		}
		if (texmapname) {
			matrec.texmap = strdup(texmapname);
			/* fill in some default sizes */
			matrec.twidth = matrec.theight = 64;

			/* try to get the actual sizes & colors from the Targa file */
			if (read_targa(&matrec, 1) < 0) {
				/* an error occured; this isn't a valid texture map */
				matrec.texmap = (char *)0;
			}
		} else {
			matrec.texmap = (char *)0;
		}

		if (verbose)
			fprintf(stdout, "Adding material %s\n", matrec.name);
		AddMaterial(&matrec);
	}

}


static COLOR
*get3dscolor(p)
	unsigned char *p;
{
	short cmd;
	static COLOR ColorBuffer;

	cmd = getshort(p); p += 6L;	/* skip id+length */

	switch (cmd) {
	case COLOR_F:
		ColorBuffer.red = getfloat(p); p+= 4L;	
		ColorBuffer.green = getfloat(p); p+= 4L;	
		ColorBuffer.blue = getfloat(p); p+= 4L;	
		break;
	case COLOR_24:
		ColorBuffer.red = (float) ((unsigned char)p[0]) / 256.0;
		ColorBuffer.green = (float) ((unsigned char)p[1]) / 256.0;
		ColorBuffer.blue = (float) ((unsigned char)p[2]) / 256.0;
		break;
	}

	return &ColorBuffer;
}

/*
 *	get a 32 bit float from a uint32_t pointer (byte swapped)
 */
static	float
getfloat(p)
	uint8_t *p;
{
	float *flp;

	*((uint32_t *)p) = getlong(p);	/* looks weird, but we don't want */
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

	return (((uint16_t)p[1] << 8) | (p[0]));
}

static uint32_t
getlong(void *p0)
{
	unsigned char *p = p0;
	uint32_t d;

	d = (((uint32_t)p[3] << 24)|((uint32_t)p[2] << 16)|((uint16_t)p[1] << 8) | p[0]);

	return d;
}

/* 
 *	build the point & face records
 */
static int
buildfacerecs(mstart, mend)
	uint8_t *mstart;	/* start of mdata section */
	uint8_t *mend;	/* ...and its end */
{
	int i;
	long length;
	uint8_t *ntri, *ntriend;		/* n-tri object */
	uint8_t *face, *faceend;		/* face array chunk */
	uint8_t *p;
	Vertex vert;
	_Polygon poly;
	int vertbase;			/* base of vertex list */
	int polybase;			/* base of polygon list */
	int numverts;
	int numpolys;
	char *matname;			/* material name */
	Object *curobj = NULL;
	Matrix M;			/* orientation matrix */
	extern char *defaultlabel;
	extern int clabels;

	if (!multiobject) {
		curobj = CreateObject( clabels ? defaultlabel + 1 : defaultlabel );
	}

	for(;;) {
		if (multiobject)
			ntri = getntriobj(mstart, mend, &length, &curobj);
		else
			ntri = getntriobj(mstart, mend, &length, (Object **)0);

		if (ntri == NULL) break;

		if (verbose)
			fprintf(stderr, "Building face records for %s\n", curobj->name);

		mstart = ntriend = ntri+length;
		vertbase = curobj->numVerts;
		polybase = curobj->numPolys;

		/* get mesh matrix */
		p = getchunk(ntri, ntriend, MSH_MATRIX, &length);
		if (p) {
			p = get3dpoint(p, &M.xrite, &M.yrite, &M.zrite);
			p = get3dpoint(p, &M.xdown, &M.ydown, &M.zdown);
			p = get3dpoint(p, &M.xhead, &M.yhead, &M.zhead);
			p = get3dpoint(p, &M.xposn, &M.yposn, &M.zposn);
#ifdef DEBUG_KF
			printf("Matrix for %s:\n", curobj->name);
			printf("%f, %f, %f, %f\n", M.xrite, M.xdown, M.xhead, M.xposn);
			printf("%f, %f, %f, %f\n", M.yrite, M.ydown, M.yhead, M.yposn);
			printf("%f, %f, %f, %f\n", M.zrite, M.zdown, M.zhead, M.zposn);
#endif
		} else {
			M = Identity;
		}

		if (animflag) {
			/* save the orientation matrix for animation info */
			curobj->inpptr = malloc(sizeof(Matrix));
			*(Matrix *)curobj->inpptr = M;
		}

		/* now build point records */
		if ((p = getchunk(ntri, ntriend, POINT_ARRAY, &length)) == NULL) {
			fprintf(stderr, "%s: points array not found\n", infilename);
			return -1;	
		}
		numverts = getshort(p); p+=2L;
		vert.vx = vert.vy = vert.vz = 0;
		vert.u = vert.v = 0;
		for (i = 0; i < numverts; i++) {
			double x, y, z;

			p = get3dpoint(p, &x, &y, &z);

			/* re-orient and scale the point */
			vert.x = x/scale;
			vert.y = -z/scale; 
			vert.z = y/scale;
			AddVertex(curobj, &vert);
		}

		/* next get the faces */
		p = face = getchunk(ntri, ntriend, FACE_ARRAY, &length);
		if (p == NULL) {
			fprintf(stderr, "%s: face array not found\n", infilename);
			return -1;
		}
		faceend = face+length;

		numpolys = getshort(p); p += 2;

		for (i = 0; i < numpolys; i++) {
			poly.material = -1;
			poly.numverts = 3;
			poly.vert[0] = vertbase + getshort(p); p += 2;
			poly.vert[2] = vertbase + getshort(p); p += 2;
			poly.vert[1] = vertbase + getshort(p); p += 2;
			poly.u[0] = poly.v[0] = 0.0;
			poly.u[2] = 0.0; poly.v[2] = 0.0;
			poly.u[1] = 0.0; poly.v[1] = 0.0;
			p += 2;		/* skip flags */

			CalcFaceNormal(curobj, &poly);
			AddPolygon(curobj, &poly);
		}

		/* get material groups and texture coordinates here! */
		if (verbose)
			fprintf(stdout, "Getting material groups\n");

		while ((p = getchunk(p, faceend, MSH_MAT_GROUP, &length)) != NULL) {
			int curmat;

			matname = (char *)p;
			while (*p) p++;			/* skip name */
			p++;				/* skip trailing 0 */
			curmat = GetMaterial(matname);
			numpolys = getshort(p); p += 2;
			for (i = 0; i < numpolys; i++) {
				int polyidx;

				polyidx = polybase + getshort(p); p += 2;
				curobj->polytab[polyidx].material = curmat;
			}
		}

		/* look for texture coordinates */
		p = getchunk(ntri, ntriend, TEX_VERTS, &length);
		if (p) {
			numverts = getshort(p); p += 2;
			for (i = 0; i < numverts; i++) {
				double u,v;
				u = getfloat(p); p += 4;
				v = 1.0 - getfloat(p); p += 4;		/* 3DS is weird! */
				if (u < 0.0)
					u = 0.0;
				else if (u > 1.0)
					u = 1.0;
				if (v < 0.0)
					v = 0.0;
				else if (v > 1.0)
					v = 1.0;

				curobj->verttab[i+vertbase].u = u;
				curobj->verttab[i+vertbase].v = v;
			}
		}

	}

	return 0;
}


static	uint8_t
*getntriobj(mstart, mend, length, objptr)
	uint8_t *mstart, *mend;
	long *length;
	Object **objptr;
{
	int done;
	uint8_t *nobj, *nobjend;	/* name object */
	uint8_t *ntriobj;		/* n-tri object */
	uint8_t *p;			/* points array */
	int i;
	char *objname = NULL;

	/*
	 *	let's find a named, n-tri object
	 */
	ntriobj = 0;
	p = mstart;
	done = 0;
	do {
		if ((nobj = getchunk(p, mend, NAMED_OBJECT, length)) == NULL) {
			done = -1;
			break;
		}
		p = nobj + *length;

		objname = (char *)nobj;
		for (i = 0; *nobj && (i < 512); i++) /* skip object's name */
			nobj++;
		nobj++;	i++;
		*length -= i;
		nobjend = nobj + *length;
	
		if ((ntriobj = getchunk(nobj, nobjend, N_TRI_OBJECT, length))
		 != NULL) {
			done = 1;
		}

	} while (!done);

	if (done == -1)		/* no n-tri object found */
		return NULL;
	else {
		if (objptr)
			*objptr = CreateObject( objname );
		return ntriobj;
	}
}

/*
 *	get a specific chunk
 */

static uint8_t
*getchunk(fbp, fbend, id, length)
	uint8_t *fbp, *fbend;		/* start & end of buffer */
	unsigned id;			     	/* chunk id */
	long * length;		     		/* return size of chunk in here */
{
	int32_t chunklen = 0L;
	unsigned chunkid;

#ifdef DEBUGCHUNK
fprintf(stderr, "getchunk: looking for (%04x)\n", id);
#endif

	while (fbp < fbend) {
		chunkid = getshort(fbp);
#ifdef DEBUGCHUNK
fprintf(stderr, "\tlooked at %04x\n", chunkid);
#endif
		fbp += 2;
		chunklen = getlong(fbp) - 6;
		fbp += 4;
		if (chunkid == id) {
			*length = chunklen;
			return fbp;
		}
		fbp += chunklen;
	}
	return NULL;
}

/*
 * get any chunk whose id appears in the
 * array "idset". "idset" is terminated
 * by an id of 0
 */

static uint8_t
*getchunkfromset(fbp, fbend, idset, length, foundid)
	uint8_t *fbp, *fbend;		/* start & end of buffer */
	unsigned *idset;		     	/* chunk id set */
	long * length;		     		/* return size of chunk in here */
	unsigned *foundid;			/* return which id was found here */
{
	uint32_t chunklen = 0L;
	unsigned chunkid;
	int i;

	while (fbp < fbend) {
		chunkid = getshort(fbp);
		fbp += 2;
		chunklen = getlong(fbp) - 6;
		fbp += 4;
		for (i = 0; idset[i]; i++) {
			if (chunkid == idset[i]) {
				*length = chunklen;
				*foundid = chunkid;
				return fbp;
			}
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
	*x =  (getfloat(p));
	p += 4L;
	*y =  (getfloat(p));
	p += 4L;
	*z =  (getfloat(p));
	p += 4L;

	return p;
}

/*
 * build key frame data information
 */

typedef struct frame {
	/* flags for whether this is a position or rotation key frame */
	int isposkf;
	int isrotkf;

	/* position */
	double xpos, ypos, zpos;
	/* rotation */
	double xaxis, yaxis, zaxis, angle;
} Frame;

typedef struct ObjKFdata {
	Object *obj;		/* object this key frame stuff applies to */
	Frame *frames;
	double pivx, pivy, pivz;	/* pivot point */
	struct ObjKFdata *next;	/* make this a linked list */
	int kfdatanum;		/* identifying integer for this data in file */
} KFdata;

/* create new keyframe data */
static KFdata *
NewKF(char *name, int numframes)
{
	Object *o;
	KFdata *kf;
	int i;

	o = FindObject(name);
	if (!o) {
		fprintf(stderr, "ERROR: Object `%s' referenced in key frame data does not exist.\n", name);
		exit(1);
	}

	kf = malloc(sizeof(KFdata));
	kf->obj = o;
	kf->frames = malloc(numframes * sizeof(Frame));
	kf->pivx = kf->pivy = kf->pivz = 0.0;

	/* no key frames yet */
	for (i = 0; i < numframes; i++) {
		kf->frames[i].isposkf = 0;
		kf->frames[i].isrotkf = 0;
	}
	kf->next = 0;
	kf->kfdatanum = -1;
	return kf;
}

/*
 * add a new keyframe structure to a list
 */
static void
AddKF(KFdata **listptr, KFdata *entry, int kfdatanum)
{
	KFdata *list;

	entry->kfdatanum = kfdatanum;
	list = *listptr;
	if (!list) {
		*listptr = entry;
		return;
	}
	while (list->next)
		list = list->next;
	list->next = entry;
}

/*
 * find the n'th entry in a keyframe list
 */
static KFdata *
GetKF(KFdata *list, int n)
{
	while (list && list->kfdatanum != n) {
		list = list->next;
	}
	return list;
}

/*
 * interpolate between key frames
 */
static void
InterpolateKF(KFdata *kflist, int numframes) {
	KFdata *kfcur;
	int i0, i1, i;

	for (kfcur = kflist; kfcur; kfcur = kfcur->next) {
		double x0, y0, z0, a0;
		double x1, y1, z1, a1;
	/* first, do position */
		if (!kfcur->frames[0].isposkf) {
			fprintf(stderr, "ERROR in data for `%s': first frame is not a key frame\n", kfcur->obj->name);
			exit(1);
		}
		if (!kfcur->frames[numframes-1].isposkf) {
		/* find the last key frame */
			for (i = numframes-1; !kfcur->frames[i].isposkf; --i)
				;
			kfcur->frames[numframes-1].xpos = kfcur->frames[i].xpos;
			kfcur->frames[numframes-1].ypos = kfcur->frames[i].ypos;
			kfcur->frames[numframes-1].zpos = kfcur->frames[i].zpos;
			kfcur->frames[numframes-1].isposkf = 1;
		}
		i0 = 0;
		x0 = kfcur->frames[0].xpos;
		y0 = kfcur->frames[0].ypos;
		z0 = kfcur->frames[0].zpos;
		i1 = 1;
		while (i1 < numframes) {
			if (kfcur->frames[i1].isposkf) {
				x1 = kfcur->frames[i1].xpos;
				y1 = kfcur->frames[i1].ypos;
				z1 = kfcur->frames[i1].zpos;
				for (i = i0+1; i < i1; i++) {
					kfcur->frames[i].xpos = x0 + (i-i0)*(x1-x0)/(double)(i1-i0);
					kfcur->frames[i].ypos = y0 + (i-i0)*(y1-y0)/(double)(i1-i0);
					kfcur->frames[i].zpos = z0 + (i-i0)*(z1-z0)/(double)(i1-i0);
				}
				i0 = i1;
				x0 = x1;
				y0 = y1;
				z0 = z1;
			}
			i1++;
		}

	/* next, do rotation */
		if (!kfcur->frames[0].isrotkf) {
			fprintf(stderr, "ERROR: first frame is not a key frame\n");
			exit(1);
		}
		if (!kfcur->frames[numframes-1].isrotkf) {
		/* find the last key frame */
			for (i = numframes-1; !kfcur->frames[i].isrotkf; --i)
				;
			kfcur->frames[numframes-1].xaxis = kfcur->frames[i].xaxis;
			kfcur->frames[numframes-1].yaxis = kfcur->frames[i].yaxis;
			kfcur->frames[numframes-1].zaxis = kfcur->frames[i].zaxis;
			kfcur->frames[numframes-1].angle = kfcur->frames[i].angle;
			kfcur->frames[numframes-1].isrotkf = 1;
		}

		i0 = 0;
		x0 = kfcur->frames[0].xaxis;
		y0 = kfcur->frames[0].yaxis;
		z0 = kfcur->frames[0].zaxis;
		a0 = kfcur->frames[0].angle;
		i1 = 1;
		while (i1 < numframes) {
			if (kfcur->frames[i1].isposkf) {
				x1 = kfcur->frames[i1].xaxis;
				y1 = kfcur->frames[i1].yaxis;
				z1 = kfcur->frames[i1].zaxis;
				a1 = kfcur->frames[i1].angle;
			/* avoid interpolating through a degenerate axis */
				if ( (x0*x1 + y0*y1 + z0*z1) < 0.0 ) {
					x1 = -x1;
					y1 = -y1;
					z1 = -z1;
					a1 = -a1;
				}
				for (i = i0+1; i < i1; i++) {
					kfcur->frames[i].xaxis = x0 + (i-i0)*(x1-x0)/(double)(i1-i0);
					kfcur->frames[i].yaxis = y0 + (i-i0)*(y1-y0)/(double)(i1-i0);
					kfcur->frames[i].zaxis = z0 + (i-i0)*(z1-z0)/(double)(i1-i0);
					kfcur->frames[i].angle = a0 + (i-i0)*(a1-a0)/(double)(i1-i0);
				}
				i0 = i1;
				x0 = x1;
				y0 = y1;
				z0 = z1;
			}
			i1++;
		}
	}
}

/*
 * convert key frames to matrices
 */
static void
ConvertKF(KFdata *kflistptr, int numframes)
{
	Object *obj;
	KFdata *kfcur;
	int i;
	Matrix A, B, C;
	Matrix Root, RootInv;
	double x, y, z, s;
	double c, t;

	for (kfcur = kflistptr; kfcur; kfcur = kfcur->next) {
		obj = kfcur->obj;
		obj->numframes = numframes;
		obj->frames = malloc(numframes * sizeof(Matrix));
	}

	for (i = 0; i < numframes; i++) {
#ifdef DEBUG_KF
		printf("\nFrame %d:\n", i);
#endif
		for (kfcur = kflistptr; kfcur; kfcur = kfcur->next) {
			obj = kfcur->obj;

			Root = *(Matrix *)obj->inpptr;
			RootInv = MatInv(Root);

			/* set A = translation from pivot point to origin */
			A = Identity;
			A.xposn = -kfcur->pivx;
			A.yposn = -kfcur->pivy;
			A.zposn = -kfcur->pivz;

		/* set B = rotation about given axis by given angle */

			/* find a unit vector along the axis */
			x = kfcur->frames[i].xaxis;
			y = kfcur->frames[i].yaxis;
			z = kfcur->frames[i].zaxis;
			s = sqrt(x*x + y*y + z*z);
			x /= s;
			y /= s;
			z /= s;

			s = sin(kfcur->frames[i].angle);
			c = cos(kfcur->frames[i].angle);
			t = 1.0 - c;

			/* matrix form for rotation around an axis */
			/* from "Graphics Gems I", page 466 */

			B.xrite = t*x*x + c;
			B.xdown = t*x*y + s*z;
			B.xhead = t*x*z - s*y;

			B.yrite = t*x*y - s*z;
			B.ydown = t*y*y + c;
			B.yhead = t*y*z + s*x;

			B.zrite = t*x*z + s*y;
			B.zdown = t*y*z - s*x;
			B.zhead = t*z*z + c;

			B.xposn = B.yposn = B.zposn = 0.0;


		/* set C = composite of A^{-1} * B * A */
			C = MMult(B, A);
			A.xposn = -A.xposn;
			A.yposn = -A.yposn;
			A.zposn = -A.zposn;
			C = MMult(A,C);

		/* apply the translation specified earlier */
			A.xposn = kfcur->frames[i].xpos - kfcur->pivx;
			A.yposn = kfcur->frames[i].ypos - kfcur->pivy;
			A.zposn = kfcur->frames[i].zpos - kfcur->pivz;

			C = MMult(A,C);
#ifdef DEBUG_KF
			{
			B = C;
printf("%s: %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f\n", obj->name, B.xrite, B.yrite, B.zrite,
B.xdown, B.ydown, B.zdown, B.xhead, B.yhead, B.zhead, B.xposn, B.yposn, B.zposn);
			}
#endif
#if 1
			if (obj->parent) {
				Root = *(Matrix *)(obj->parent->inpptr);
				C = MMult(Root,C);
			}
			C = MMult(C, RootInv);
#endif
		/* lastly, convert from 3D Studio coordinate system to ours */
			C = MMult(C, Coord3DSInv);
			obj->frames[i] = MMult(Coord3DS, C);
		}
	}
}

static int
buildkfdata(uint8_t *kstart, uint8_t *kend)
{
	int i;
	int32_t numframes;
	long length;
	int32_t numkeys;
	uint8_t *p;
	uint8_t *onode, *onodeend;
	double x, y, z, angle;
	double pivx, pivy, pivz;
	int32_t frame;
	short splinebits;
	int parent;
	KFdata *kflist, *kfpar, *kfcur;
	int kfdatanum;			/* number of the current set of key frame data */
	static unsigned kfdataset[] = { OBJECT_NODE_TAG, CAMERA_NODE_TAG, TARGET_NODE_TAG, LIGHT_NODE_TAG, L_TARGET_NODE_TAG,
					SPOTLIGHT_NODE_TAG, 0};
	unsigned whichnode;

	kflist = (KFdata *)0;
	kfdatanum = 0;

	p = getchunk(kstart, kend, KFHDR, &length);
	if (!p) {
		fprintf(stderr, "ERROR: no keyframe header present in file\n");
		return -1;
	}
	p += 2;		/* skip version */
	for (i = 0; i < length-6; i++)
		p++;		/* skip file name, if any */

	numframes = getlong(p)+1;
#ifdef DEBUG_KF
printf("%ld frames\n", numframes);
printf("Getting Object Node Chunks:\n");
#endif
	onodeend = kstart;
	for(;;) {
		onode = getchunkfromset(onodeend, kend, kfdataset, &length, &whichnode);
		if (!onode) {
			break;
		}
		onodeend = onode + length;
		if (whichnode != OBJECT_NODE_TAG) {
			kfdatanum++;
			continue;
		}
		p = getchunk(onode, onodeend, NODE_HDR, &length);
		if (!p) {
			fprintf(stderr, "Missing node header in keyframe data\n");
			return -1;
		}
#ifdef DEBUG_KF
			printf("Getting keyframe data for: %s\n", p);
#endif
		kfcur = NewKF((char *)p, numframes);
		AddKF(&kflist, kfcur, kfdatanum++);

		p += length-2;		/* skip name and flags */
		parent = (short)getshort(p);
		if (parent >= 0) {
			kfpar = GetKF(kflist, parent);
			if (!kfpar) {
				fprintf(stderr, "ERROR: bad key frame index (%d)\n", parent);
			} else {
#ifdef DEBUG_KF
				printf("  parent is %s\n", kfpar->obj->name);
#endif
				kfcur->obj->parent = kfpar->obj;
				kfcur->obj->siblings = kfpar->obj->children;
				kfpar->obj->children = kfcur->obj;
			}
		} else {
			kfpar = 0;
		}

		p = getchunk(onode, onodeend, PIVOT, &length);
		pivx = pivy = pivz = 0.0;
		if (p) {
			get3dpoint(p, &pivx, &pivy, &pivz);
		}
#ifdef DEBUG_KF
printf("  pivot: %f, %f, %f\n", pivx, pivy, pivz);
#endif

		kfcur->pivx = pivx;
		kfcur->pivy = pivy;
		kfcur->pivz = pivz;

		p = getchunk(onode, onodeend, POS_TRACK_TAG, &length);
		if (p) {
			/* get track header */
			p += 10;		/* skip internal stuff */
			numkeys = getlong(p);
			p += 4;
			for (i = 0; i < numkeys; i++) {
				short j;

				/* get key header */
				frame = getlong(p);
				if ((frame < 0) || (frame >= numframes)) {
					fprintf(stderr, "ERROR: bad frame number (%" PRId32 ") in keyframe data for `%s'\n",
						frame, kfcur->obj->name);
					exit(1);
				}
				p += 4;
				splinebits = getshort(p);
				p += 2;
				/* skip spline data */
				for (j = 0; j < 16; j++) {
					if (splinebits & 1)
						p += 4;
					splinebits = splinebits >> 1;
				}
				/* print position */
				p = get3dpoint(p, &x, &y, &z);
#ifdef DEBUG_KF
				printf("frame %ld: point %f, %f, %f\n", frame, x, y, z);
#endif
				kfcur->frames[frame].isposkf = 1;
				if (kfpar) {
					x += kfpar->pivx;
					y += kfpar->pivy;
					z += kfpar->pivz;
				}
				kfcur->frames[frame].xpos = x;
				kfcur->frames[frame].ypos = y;
				kfcur->frames[frame].zpos = z;
			}
		}

		p = getchunk(onode, onodeend, ROT_TRACK_TAG, &length);
		if (p) {
			/* get track header */
			p += 10;		/* skip internal stuff */
			numkeys = getlong(p);
			p += 4;
			for (i = 0; i < numkeys; i++) {
				short j;

				/* get key header */
				frame = getlong(p);
				p += 4;
				splinebits = getshort(p);
				p += 2;
				/* skip spline data */
				for (j = 0; j < 16; j++) {
					if (splinebits & 1)
						p += 4;
					splinebits = splinebits >> 1;
				}
				/* print position */
				angle = getfloat(p);
				p += 4;
				p = get3dpoint(p, &x, &y, &z);
#ifdef DEBUG_KF
				printf("frame %ld: rotate %f degrees around %f, %f, %f\n", frame, angle, x, y, z);
#endif
				kfcur->frames[frame].isrotkf = 1;
				kfcur->frames[frame].xaxis = x;
				kfcur->frames[frame].yaxis = y;
				kfcur->frames[frame].zaxis = z;
				kfcur->frames[frame].angle = angle;
			}
		}
	}

/* interpolate between key frames */
	InterpolateKF(kflist, numframes);

/* now convert key frames to matrices */
	ConvertKF(kflist, numframes);

#ifdef DEBUG_KF
printf("Done keyframe stuff\n");
#endif
	return 0;
}
