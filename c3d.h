/*
 * Data structures for 3D renderer
 */

/*
 * a light can be of 1 of two types:
 * (1) an "in scene" light: this has attenuation based on distance to
 *	the point being illuminated
 * (2) sunlight: this is just a vector
 */

typedef struct light {
#ifdef USE_FLOATS
	float	x,y,z;		/* light position, for in scene, or normal vector for sunlight */
	float	bright;
#else
	short	x,y,z;		/* light position, for in scene, or normal vector for sunlight */
	unsigned short bright;		/* base intensity, for in scene, or 0 for sunlight */
#endif
} Light;

/*
 * a lighting model consists of:
 * (1) ambient illumination (from 0-$ffff)
 * (2) some number of lights
 *
 * we (arbitrarily) impose a limit of MAXLIGHTS lights
 * per scene.
 */

#define MAXLIGHTS 8

typedef struct lightmodel {
#ifdef USE_FLOATS
	float	ambient;
#else
	unsigned short	ambient;	/* ambient illumination */
#endif
	short	numlights;		/* number of lights */
	Light	lights[MAXLIGHTS];		/* pointer to lights */
} Lightmodel;

/*
 * transformation matrix: a 4x4 matrix, last column is always 0 0 0 1 so is not stored
 * numbers are 0.14 fractions (1 = 0x4000)
 */

#ifdef USE_FLOATS
#define frac float
#else
#define frac short
#endif

typedef struct matrix {
	frac	xrite,yrite,zrite;
	frac	xdown,ydown,zdown;
	frac	xhead,yhead,zhead;
	frac	xposn,yposn,zposn;
} Matrix;

/*
 * points consist of the point + its vertex normal
 */
typedef struct point {
	frac	x, y, z;
	frac	vx, vy, vz;
} Point;


typedef unsigned char Pixel;

/*
 * a bitmap consists of:
 * (1) the width and height of the map
 * (2) line length
 * (3) a pointer to the actual data
 */

typedef struct texmap {
	short	width, height;
	long	linelen;
	Pixel   *data;
} Bitmap;

/*
 * a "material" consists of:
 * a color (used for Gouraud shading)
 * flags (currently unused, set to 0)
 * a pointer to a bitmap used for texture
 * mapping
 */
typedef struct material {
	unsigned short	color;
	unsigned short	flags;
	Bitmap		*tmap;
} Material;

#if 0
/* data structure for a face */
typedef struct triangle {
	short	npts;			/* number of points (same as # of sides) */
	short	material;		/* index into Materials table */

	short	fx, fy, fz, fd;		/* plane equation for polygon (face normal and -normal*point) */


/* for each point, keep:
   (1) the index of the point into the points or tpoints table (2 bytes)
   (2) texture map coordinates (U,V) (1 byte for each of U and V)
   Thus, the points[] array has 2 words (4 bytes) per point
 */
	unsigned short points[6];
} Face;
#else
typedef short Face;
#endif

/* object data: many objects may share the same object data (e.g. if there are 3 identical spaceships
   flying around, all 3 objects can use the same data) */

typedef struct objdata {
	short	numpolys;		/* number of faces in object */
	short	numpoints;		/* number of points in object */
	short	nummaterials;		/* number of entries in the materials table */
	short	reserved;		/* reserved for future expansion, set to 0 */
	Face	*faces;			/* pointer to polygons */
	Point	*points;		/* point table */
	Material *materials;		/* pointer to table of materials (e.g. colors) */
} C3DObjdata;

/* finally, an object: a transformation matrix, pointer to object data, plus
 * whatever else we eventually decide to include.
 */

typedef struct cobject {
	C3DObjdata	*data;
	Matrix	M;			/* object space -> world space  transformation */
} C3DObject;
