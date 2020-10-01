/*
 * internal format data structures, etc.
 */

#ifndef EXTERN
#define EXTERN extern
#endif

typedef struct material {
	char *name;			/* material name */
	int red, green, blue;		/* color components */
	char *texmap;			/* texture map name, or 0 if no texture */
	int twidth, theight;		/* height and width of texture, if known (we look for a .TGA file) */
} Material;

typedef struct vertex {
	double	x, y, z;		/* coordinates of the point */
	double  vx, vy, vz;		/* vertex normal for point */
	double	u,v;			/* texture coordinates for point */
} Vertex;

#define MAXVERTICES 8
typedef struct polygon {
	int material;			/* material for this polygon */
	double fx, fy, fz;		/* face normal */
	int numverts;			/* number of vertices in the polygon */
	int vert[MAXVERTICES];		/* vertex indicies, in clockwise order */
	double u[MAXVERTICES];		/* texture coordinates (in same order as vertices) */
	double v[MAXVERTICES];
} Polygon;



/*
 * transformation matrix: a 4x4 matrix, last column is always 0 0 0 1 so is not stored
 */
typedef struct matrix {
	double	xrite,yrite,zrite;
	double	xdown,ydown,zdown;
	double	xhead,yhead,zhead;
	double	xposn,yposn,zposn;
} Matrix;

typedef struct object {
	char *name;			/* name of this mesh */
	double pivotx, pivoty, pivotz;	/* origin for rotations */

	Vertex *verttab;		/* vertex table */
	int numVerts;			/* number of vertices currently in table */
	int maxVerts;			/* current size of vertex table */

	Polygon *polytab;		/* face table for this object */
	int numPolys;			/* number of faces currently in table */
	int maxPolys;			/* current size of face table */

	/* hierarchy info */
	struct object *parent;		/* object at higher level */
	struct object *children;	/* objects at lower level */
	struct object *siblings;	/* objects at same level */

	/* animation info */
	int numframes;			/* number of frames of animation */
	Matrix *frames;			/* pointer to the frames */

	/* private data for input functions */
	void	*inpptr;		/* used by e.g. 3dsfile.c, lwfile.c */
} Object;


EXTERN	Material *mattab;		/* material table */
EXTERN	int numMaterials;		/* number of materials currently in table */
EXTERN	int maxMaterials;		/* current size of materials table */


EXTERN Object *objtab;			/* object table */
EXTERN int numObjs;			/* number of objects currently in table */

