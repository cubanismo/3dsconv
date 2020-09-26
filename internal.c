#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef __DUMB_MSDOS__
#include <alloc.h>
#define myrealloc farrealloc
#define mycalloc farcalloc
#define myfree farfree
#else
#define myrealloc realloc
#define mycalloc calloc
#define myfree free
#endif

#ifdef _WIN32
#define strdup _strdup
#endif

#define EXTERN
#include "internal.h"

extern int verbose;

/*
 * Add a material to the global "mattab" array.
 * this can involve reallocating that array, if there
 * isn't enough room in it now
 */

void
AddMaterial( Material *mat )
{
	int i;

	/* sanity check: is the material already in the table? */
	for (i = 0; i < numMaterials; i++) {
		if (!strcmp(mattab[i].name, mat->name)) {
			fprintf(stderr, "Error: duplicate material name (%s)\n", mat->name);
			return;
		}
	}
	numMaterials++;
	if (numMaterials > maxMaterials) {
		/* expand the table */
		maxMaterials += 32;
		mattab = myrealloc(mattab, maxMaterials*sizeof(Material));
		if (!mattab) {
			fprintf(stderr, "FATAL ERROR: out of memory\n");
			exit(2);
		}
	}
	mattab[numMaterials-1] = *mat;
}

/*
 * look for a material in the materials table, and return its index
 */
int
GetMaterial(char *name)
{
	int i;

	for (i = 0; i < numMaterials; i++) {
		if (!strcmp(mattab[i].name, name)) {
			return i;
		}
	}

	fprintf(stderr, "Warning: material (%s) not found\n", name);
	return 0;
}

/*
 * Add a new vertex to the vertex list for
 * a specific object.
 */

void
AddVertex( Object *obj, Vertex *vert )
{
	obj->numVerts++;
	if (obj->numVerts > obj->maxVerts) {
		/* expand the table */
		obj->maxVerts += 64;
		obj->verttab = myrealloc(obj->verttab, obj->maxVerts*sizeof(Vertex));
		if (!obj->verttab) {
			fprintf(stderr, "FATAL ERROR: out of memory\n");
			exit(2);
		}
	}
	obj->verttab[obj->numVerts-1] = *vert;
}

/*
 * Add a new polygon to an object's polygon list.
 */

void
AddPolygon( Object *obj, _Polygon *p )
{
	obj->numPolys++;
	if (obj->numPolys > obj->maxPolys) {
		/* expand the table */
		obj->maxPolys += 64;
		obj->polytab = myrealloc(obj->polytab, obj->maxPolys*sizeof(_Polygon));
		if (!obj->polytab) {
			fprintf(stderr, "FATAL ERROR: out of memory\n");
			exit(2);
		}
	}
	obj->polytab[obj->numPolys-1] = *p;
}


/*
 * various utility routines for operating on
 * internal format representation
 */

/*
 * Calculate the face normal of a polygon.
 * Uses Newell's method (see Graphics Gems III)
 */

void
CalcFaceNormal(Object *obj, _Polygon *P)
{
	int i;
	Vertex *p0, *p1;	/* start and end points */
	double vx, vy, vz;
	double length;

	vx = vy = vz = 0.0;

	p0 = &obj->verttab[P->vert[P->numverts-1]];
	for (i = 0; i < P->numverts; i++) {
		p1 = &obj->verttab[P->vert[i]];
		vx += (p1->y - p0->y) * (p1->z + p0->z);
		vy += (p1->z - p0->z) * (p1->x + p0->x);
		vz += (p1->x - p0->x) * (p1->y + p0->y);
		p0 = p1;
	}

	length = sqrt(vx*vx + vy*vy + vz*vz);
	vx /= length;
	vy /= length;
	vz /= length;

	P->fx = vx;
	P->fy = vy;
	P->fz = vz;
}


/*
 * Build vertex normals for every point in a given
 * object.
 * For now, we use a very simple algorithm: the vertex
 * normal is the average of the face normals of all
 * faces that share the vertex.
 */

void
CalcVertexNormals( Object *obj )
{
	int i, j;		/* loop counters */
	_Polygon *P;
	Vertex *V;
	double length;

	/* first, for each vertex, add up all the polygon
	 * face normals for faces using this vertex
	 * (NOTE: we assume that the vertex normals were
	 * initialized to 0!)
	 */
	for (i = 0; i < obj->numPolys; i++) {
		P = &obj->polytab[i];
		for (j = 0; j < P->numverts; j++) {
			V = &obj->verttab[P->vert[j]];
			V->vx += P->fx;
			V->vy += P->fy;
			V->vz += P->fz;
		}
	}

	/* now normalize all the face normals */
	for (i = 0; i < obj->numVerts; i++) {
		V = &obj->verttab[i];
		length = sqrt(V->vx*V->vx + V->vy*V->vy + V->vz*V->vz);
		if (length > 0.0) {
			V->vx /= length;
			V->vy /= length;
			V->vz /= length;
		}
	}
}

/*
 * Merge all vertices that are "sufficiently close".
 */
static int
PointsSame(Vertex *v1, Vertex *v2)
{
	double dist;
	extern double pointdelta;

	dist = fabs(v1->x - v2->x) + fabs(v1->y - v2->y) +
		fabs(v1->z - v2->z);
	if (dist < pointdelta)
		return 1;
	else
		return 0;
}

void
MergeVertices( Object *obj )
{
	int *pointmap;
	int i, j;
	_Polygon *P;
	int newnumVerts;
	Vertex *newverttab;

	/* first, save the (u,v) information into the polygon structure */
	for (i = 0; i < obj->numPolys; i++) {
		P = &obj->polytab[i];
		for (j = 0; j < P->numverts; j++) {
			P->u[j] = obj->verttab[P->vert[j]].u;
			P->v[j] = obj->verttab[P->vert[j]].v;
		}
	}

	pointmap = mycalloc( obj->numVerts, sizeof(int) );
	if (!pointmap) {
		fprintf(stderr, "WARNING: unable to merge vertices (out of memory)\n");
		return;
	}
	newverttab = mycalloc( obj->numVerts, sizeof(Vertex) );
	if (!newverttab) {
		myfree(pointmap);
		fprintf(stderr, "WARNING: unable to merge vertices (out of memory)\n");
		return;
	}

	/* for each point, see if it is approximately the same as
	 * a point occuring earlier in the list
	 */
	newnumVerts = 0;
	for (i = 0; i < obj->numVerts; i++) {
		for (j = 0; j < newnumVerts; j++) {
			if (PointsSame(&obj->verttab[i], &newverttab[j])) {
				pointmap[i] = j;
				goto skippt;
			}
		}
		pointmap[i] = newnumVerts;
		newverttab[newnumVerts++] = obj->verttab[i];
skippt:
		;
	}

	/* did we merge points? if so, relabel all the polygon vertices */
	if (newnumVerts != obj->numVerts) {
		if (verbose)
			fprintf(stdout, "Object %s: merged %d points into %d\n", obj->name, obj->numVerts, newnumVerts);
		myfree(obj->verttab);
		obj->verttab = newverttab;
		obj->numVerts = newnumVerts;
		for (i = 0; i < obj->numPolys; i++) {
			P = &obj->polytab[i];
			for (j = 0; j < P->numverts; j++) {
				P->vert[j] = pointmap[P->vert[j]];
			}
		}
	} else {
		myfree(newverttab);
	}
	myfree(pointmap);
}


/*
 * merge triangles into quadrilaterals, if possible
 */
/*
 * Two triangles can be merged iff:
 * (1) They have the same materials.
 * (2) They have the same face normal.
 * (3) They share an edge.
 * (4) Texture coordinates match along the edge.
 * (5) The resulting polygon is convex
 */

/* the convexity test is implemented by comparing the cross product of
 * each pair of adjoining vertices with the face normal; if
 * they point in different directions, the polygon is not convex.
 */
static int
Convex( Vertex *verttab, _Polygon *A )
{
	Vertex *VA, *VB, *VC;
	double vx, vy, vz;	/* cross product */
	int i;

	VA = &verttab[A->vert[A->numverts-2]];
	VB = &verttab[A->vert[A->numverts-1]];

	for (i = 0; i < A->numverts; i++) {
		VC = &verttab[A->vert[i]];

		vx = (VA->y - VB->y)*(VC->z - VB->z) - (VA->z - VB->z)*(VC->y - VB->y);
		vy = (VA->z - VB->z)*(VC->x - VB->x) - (VA->x - VB->x)*(VC->z - VB->z);
		vz = (VA->x - VB->x)*(VC->y - VB->y) - (VA->y - VB->y)*(VC->x - VB->x);

		/* check dot product with face normal */
		if (vx * A->fx + vy * A->fy + vz * A->fz < 0.1)
			return 0;
		VA = VB;
		VB = VC;
	}
	return 1;
}

static int
CanMerge( Vertex *verttab, _Polygon *A, _Polygon *B, _Polygon *Merged )
{
	extern double facedelta;
	double normdiff;
	int i, j, k;
	int Astart, Aend;		/* start and end points of current edge of A */
	int Bstart, Bend;		/* start and end points of currend edge of B */

	if (A->material != B->material)
		return 0;

	normdiff = fabs(A->fx - B->fx) + fabs(A->fy - B->fy) + fabs(A->fz - B->fz);
	if (normdiff > facedelta)
		return 0;

	/* for each edge of A, see if there is a corresponding edge of B */
	Astart = A->vert[A->numverts-1];
	for (i = 0; i < A->numverts; i++) {
		Aend = A->vert[i];
		Bstart = B->vert[B->numverts-1];
		for (j = 0; j < B->numverts; j++) {
			Bend = B->vert[j];
			if (Bstart == Aend && Bend == Astart) {
			/* OK, there's a matching edge. Go to the
			   point copying routine */
				goto mergepolys;
			}
			Bstart = Bend;
		}
		Astart = Aend;
	}
	/* if we get here, no edges matched */
	return 0;

mergepolys:
/* here's where we actually merge the polygons. We do this
 * by walking along polygon A until we run into the edge
 * we just matched; at that point we have to insert
 * polygon B.
 */
	k = 0;
	for (i = 0; i < A->numverts; i++) {
		Merged->vert[k] = A->vert[i];
		Merged->u[k] = A->u[i];
		Merged->v[k] = A->v[i];
		k++;
		if (A->vert[i] == Astart) {	/* here's where we insert polygon B */
		/* j was left pointing at the vertex in B corresponding to Astart, i.e. Bend */
		/* make sure texture coordinates match */
			if ( (A->u[i] != B->u[j]) || (A->v[i] != B->v[j]) )
				return 0;
			for(;;) {
				j++;
				if (j >= B->numverts)
					j = 0;
				if (B->vert[j] == Aend)		/* this is guaranteed to happen eventually */
					break;
				Merged->vert[k] = B->vert[j];
				Merged->u[k] = B->u[j];
				Merged->v[k] = B->v[j];
				k++;
			}
		}
	}
	Merged->numverts = k;
	Merged->material = A->material;
	Merged->fx = (A->fx+B->fx)/2.0;
	Merged->fy = (A->fy+B->fy)/2.0;
	Merged->fz = (A->fz+B->fz)/2.0;

/* make sure the merged triangles are still convex */
	if (Convex(verttab, Merged))
		return 1;
	else
		return 0;
}

void
MergeFaces( Object *obj )
{
	int i;
	_Polygon *FirstPoly, *NextPoly;
	_Polygon MergedPoly;
	int mergedsome;
	int oldnumPolys;

	mergedsome = 0;
	for (i = 0; i < obj->numPolys-1; ) {
		FirstPoly = &obj->polytab[i];
		NextPoly = &obj->polytab[i+1];
		if ( CanMerge( obj->verttab, FirstPoly, NextPoly, &MergedPoly ) ) {
			*FirstPoly = MergedPoly;
			NextPoly->numverts = 0;			/* mark NextPoly as deleted */
			i += 2;					/* skip both polygons */
			mergedsome++;
		} else {
			i++;
		}
	}

	/* now compress the polygon list */
	if (mergedsome) {
		oldnumPolys = obj->numPolys;
		obj->numPolys = 0;
		for (i = 0; i < oldnumPolys; i++) {
			if (obj->polytab[i].numverts > 0) {
				if (i != obj->numPolys)
					obj->polytab[obj->numPolys] = obj->polytab[i];
				obj->numPolys++;
			}
		}
		if (verbose)
			fprintf(stdout, "Object %s: merged %d triangles into %d polygons\n", obj->name,
				oldnumPolys, obj->numPolys);
	}
}

/*
 * check for uncolored faces; if any exist, add a default material to
 * the material list
 */
void
CheckUncoloredFaces()
{
	int numuncolored = 0;
	int i, j;
	Material matrec;
	Object *obj;

	for (j = 0; j < numObjs; j++) {
		obj = &objtab[j];
		for (i = 0; i < obj->numPolys; i++) {
			if ( obj->polytab[i].material == -1 ) {
				obj->polytab[i].material = numMaterials;	/* this will be the index of the default material */
				numuncolored++;
			}
		}
	}

	if (numuncolored) {
		fprintf(stderr, "Warning: %d uncolored faces\n", numuncolored);
	/* add a default material */
		matrec.red = 128;
		matrec.green = 128;
		matrec.blue = 128;
		matrec.name = strdup("Default Material");
		matrec.texmap = 0;
		AddMaterial(&matrec);
	}
}

/*
 * Find a named object in the objects
 * table, and return a pointer to it
 */
Object *
FindObject( char *name )
{
	int i;

	for (i = 0; i < numObjs; i++) {
		if (!strcmp(objtab[i].name, name)) {
			return &objtab[i];
		}
	}
	return (Object *)0;
}

/*
 * Create a new (blank) object,
 * and add it to the objects table.
 */

Object *
CreateObject( char *name )
{
	int i;
	Object *curobj;

	/* sanity check: is the object already in the table? */
	for (i = 0; i < numObjs; i++) {
		if (!strcmp(objtab[i].name, name)) {
			fprintf(stderr, "FATAL ERROR: duplicate object name (%s)\n", name);
			exit(1);
		}
	}
	numObjs++;
	/* expand the table */
	objtab = myrealloc(objtab, numObjs*sizeof(Object));
	if (!objtab) {
		fprintf(stderr, "FATAL ERROR: out of memory\n");
		exit(1);
	}
	curobj = &objtab[numObjs-1];

	curobj->name = strdup(name);
	curobj->pivotx = curobj->pivoty = curobj->pivotz = 0.0;

	curobj->verttab = 0;
	curobj->numVerts = curobj->maxVerts = 0;

	curobj->polytab = 0;
	curobj->numPolys = curobj->maxPolys = 0;

	curobj->children = curobj->siblings = curobj->parent = (Object *)0;
	curobj->numframes = 0;
	curobj->frames = (Matrix *)0;

	curobj->inpptr = (void *)0;

	return curobj;
}


/*
 * fix up the "sibling" and "parent" object lists
 * return a "root" object, i.e. one with no parent
 */

Object *
FixObjectLists( void )
{
	Object *rootobj;
	int i;

	rootobj = (Object *)0;

	for (i = 0; i < numObjs; i++) {
		if (!objtab[i].parent) {
			rootobj = &objtab[i];
			break;
		}
	}

	if (!rootobj) {
		fprintf(stderr, "ERROR: all objects in file have a parent??\n");
		exit(1);
	}

	i++;
	for (; i < numObjs; i++) {
		if (objtab[i].parent == 0) {
			objtab[i].siblings = rootobj->siblings;
			rootobj->siblings = &objtab[i];
		}
	}

	return rootobj;
}

/*
 * do a matrix multiply M = A*B, return M
 */
Matrix
MMult(Matrix A, Matrix B)
{
	Matrix M;

	M.xrite = A.xrite*B.xrite + A.xdown*B.yrite + A.xhead*B.zrite;
	M.xdown = A.xrite*B.xdown + A.xdown*B.ydown + A.xhead*B.zdown;
	M.xhead = A.xrite*B.xhead + A.xdown*B.yhead + A.xhead*B.zhead;
	M.xposn = A.xrite*B.xposn + A.xdown*B.yposn + A.xhead*B.zposn + A.xposn;

	M.yrite = A.yrite*B.xrite + A.ydown*B.yrite + A.yhead*B.zrite;
	M.ydown = A.yrite*B.xdown + A.ydown*B.ydown + A.yhead*B.zdown;
	M.yhead = A.yrite*B.xhead + A.ydown*B.yhead + A.yhead*B.zhead;
	M.yposn = A.yrite*B.xposn + A.ydown*B.yposn + A.yhead*B.zposn + A.yposn;

	M.zrite = A.zrite*B.xrite + A.zdown*B.yrite + A.zhead*B.zrite;
	M.zdown = A.zrite*B.xdown + A.zdown*B.ydown + A.zhead*B.zdown;
	M.zhead = A.zrite*B.xhead + A.zdown*B.yhead + A.zhead*B.zhead;
	M.zposn = A.zrite*B.xposn + A.zdown*B.yposn + A.zhead*B.zposn + A.zposn;

	return M;
}

static const Matrix Identity = {
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	0.0, 0.0, 0.0
};

/* find the inverse of a matrix M, and return it */
Matrix
MatInv(Matrix M)
{
	Matrix A, B;
	/* decompose the root transformation into a translation (A) and rotation (B) */

	A = Identity;
	A.xposn = -M.xposn;
	A.yposn = -M.yposn;
	A.zposn = -M.zposn;

	B = Identity;
	B.xrite = M.xrite;
	B.yrite = M.xdown;
	B.zrite = M.xhead;
	B.xdown = M.yrite;
	B.ydown = M.ydown;
	B.zdown = M.yhead;
	B.xhead = M.zrite;
	B.yhead = M.zdown;
	B.zhead = M.zhead;

	return MMult(B, A);
}

#if !defined(atarist) && !defined(_WIN32)
double rint(double x)
{
	return floor( x + 0.5 );
}
#endif

