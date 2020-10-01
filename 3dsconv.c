/*
 * 3DSCONV:
 * Convert a .3ds file to a .S file which can be assembled
 * by madmac. The output format is compatible with the
 * new 3D library.
 *
 * Copyright 1995 Atari Corporation.
 * All Rights Reserved.
 *
 * Version
 * 1.0:		first release
 * 1.1:		added -clabels, -noclabels, and -noheader options
 * 1.2:		added (preliminary) multiple object support
 * 1.3:		added Lightwave object support
 * 1.4:		added animating object support
 * 1.5:		added C output format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __DUMB_MSDOS__
#include <alloc.h>
#define mymalloc farmalloc
#define myfree farfree
#else
#define mymalloc malloc
#define myfree free
#endif

#ifndef _WIN32
#define stricmp strcasecmp
#else
#define strdup _strdup
#define stricmp _stricmp
#endif

#include "internal.h"
#include "proto.h"

#define VERSION "1.5"
#define DEFAULT_PROGNAME	"3dsconv"
char *progname;		/* the program's name */

/* global variables settable by the user */
int	output_format;
#define FORMAT_JAG	0		/* old output format */
#define	FORMAT_N3D	1		/* new output format */
#define FORMAT_ANIM	2		/* new output format + animation info */
#define FORMAT_C	3		/* C file output format, integer */
#define FORMAT_CFLOAT	4		/* C file output format, floating point */

char 	*defaultlabel;			/* label for the object data */
char 	*outfilename = (char *)0;	/* name of the output file */
char 	*infilename;			/* name of the input file */
int	merge_tris;			/* merge triangles into polygons if 1, don't if 0 */
int	verbose;			/* report lots of things about what we're doing if 1, be quiet if 0 */
int	usedataseg;			/* whether to use the data segment (1) or text segment (0) */
int	outputheader;			/* whether to output .include commands */
int	clabels;			/* output C style labels (i.e. with underbars) if this is 1 */
int	multiobject;			/* output a multiple object header (1) or just 1 object (0) */
int	animflag;			/* include animation data (1) or not (0) */

double	uscale;				/* user specified scale factor */
double	pointdelta;			/* if points are less than pointdelta apart, they are
					 * merged
					 */
double facedelta;			/* if face normals are less than this much apart, they
					 * can be merged
					 */

/* Global variables */
char *filepath;				/* path where the .3ds file is found */

void
usage( char *errmsg )
{
	if (errmsg)
		fprintf(stderr, "%s\n", errmsg);
	fprintf(stderr, "%s Version %s\n", progname, VERSION);
	fprintf(stderr, "Usage: %s [-o outfile][-l label][-f format][-scale scale] {options} inputfile\n", progname);
	fprintf(stderr, "Valid options are:\n");
	fprintf(stderr, "  -clabels:    Add an underbar to labels (default for -f new)\n");
	fprintf(stderr, "  -multiobj:   Output multiple objects, rather than merging all named objects\n");
	fprintf(stderr, "  -noclabels:  Do not add underbars to labels (default for -f old)\n");
	fprintf(stderr, "  -noheader:   Do not output .data header or .include commands at start of file\n");
	fprintf(stderr, "  -textseg:    Put model in text segment, instead of data segment\n");
	fprintf(stderr, "  -triangles:  Do not merge triangles into polygons\n");
	fprintf(stderr, "  -verbose:    Print messages about what is going on\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "In the -f command, 'format' must be one of:\n");
	fprintf(stderr, "  a3d          Animation format\n");
	fprintf(stderr, "  n3d          New 3D library output format\n");
	fprintf(stderr, "  j3d          Original jaguar 3D library format\n");
	fprintf(stderr, "  c            C data file\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	char wkstr[256];
	int i, retval;
	char *extension;

	pointdelta = 1.0;
	facedelta = 0.01;
	merge_tris = 1;
	uscale = 1.0;
	output_format = FORMAT_N3D;
	verbose = 0;
	usedataseg = 1;
	outputheader = 1;
	clabels = -1;	/* a default value, overridden later */
	multiobject = 0;
	animflag = 0;

	progname = *argv++;
	if (!*progname) {				/* if for some reason the runtime library didn't get our name... */
		progname = DEFAULT_PROGNAME;		/* assume this is our name */
	}
	argc--;
	if (!*argv) {
		usage( (char *)0 );		/* program invoked with no arguments */
	}
	while (*argv) {
		if (**argv != '-') break;
		if (!strcmp(*argv, "-o")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No output file name given with '-o'\n" );
			}
			outfilename = *argv;
		} else if (!strcmp(*argv, "-l")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No label name given with '-l'\n" );
			}
			defaultlabel = *argv;
		} else if (!strcmp(*argv, "-f")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No format type given with '-f'\n" );
			}
			if (!strcmp(*argv, "new") || !strcmp(*argv, "n3d"))
				output_format = FORMAT_N3D;
			else if (!strcmp(*argv, "old") || !strcmp(*argv, "j3d"))
				output_format = FORMAT_JAG;
			else if (!strcmp(*argv, "cf") || !strcmp(*argv, "cfloat"))
				output_format = FORMAT_CFLOAT;
			else if (!strcmp(*argv, "c") || !strcmp(*argv, "c3d"))
				output_format = FORMAT_C;
			else if (!strcmp(*argv, "anim") || !strcmp(*argv, "a3d")) {
				output_format = FORMAT_ANIM;
				animflag = multiobject = 1;
			} else
				usage( "Unknown format type given after '-f'\n" );
		} else if (!strcmp(*argv, "-scale")) {
			argv++; argc--;
			if (!*argv) {
				usage( "No scale factor given with '-scale'\n" );
			}
			uscale = atof(*argv);
		} else if (!strncmp(*argv, "-tri", 4)) {
			merge_tris = 0;
		} else if (!strcmp(*argv, "-textseg")) {
			usedataseg = 0;
		} else if (!strncmp(*argv, "-v", 2)) {
			verbose = 1;
		} else if (!strncmp(*argv, "-clabel", 5)) {
			clabels = 1;
		} else if (!strncmp(*argv, "-noclabel", 6)) {
			clabels = 0;
		} else if (!strncmp(*argv, "-noheader", 6)) {
			outputheader = 0;
		} else if (!strncmp(*argv, "-multio", 6)) {
			multiobject = 1;
		} else {
			sprintf( wkstr, "Illegal option given: '%s'\n", *argv );
			usage(wkstr);		/* illegal option */
		}
		argv++; argc--;
	}
	if (argc != 1) {		/* should be exactly one argument left, the input file name */
		usage( "Exactly one input file must be specified\n" );
	}
	infilename = *argv;
	if (!outfilename) {
		if (output_format == FORMAT_JAG)
			outfilename = change_extension(infilename, ".j3d");
		else if (output_format == FORMAT_ANIM)
			outfilename = change_extension(infilename, ".a3d");
		else if (output_format == FORMAT_C || output_format == FORMAT_CFLOAT)
			outfilename = change_extension(infilename, ".c");
		else
			outfilename = change_extension(infilename, ".n3d");
	}

	/* if neither -clabels nor -noclabels was given explicitly, default to
	 * C style labels for new output format, and non-C style for the old
	 * output format
	 */
	if (clabels == -1) {		/* option wasn't explicitly given by the user */
		clabels = (output_format == FORMAT_JAG) ? 0 : 1;
	}

	filepath = strdup(infilename);
	/* replace the last path separator with a null */
	/* also, if no label has been specified, make one out
	 * of the file name
	 */
	{
		char *s;
		char *filelabel;

		s = strrchr(filepath, '\\');
		if (!s)
			s = strrchr(filepath, '/');
		if (!s)
			s = strrchr(filepath, ':');

		if (s) {
			++s;
			filelabel = name2label(s);
			*s = 0;
		} else {
			filelabel = name2label(filepath);
			*filepath = 0;
		}
		if (!defaultlabel) {
			defaultlabel = mymalloc(strlen(filelabel)+1);
			strcpy(defaultlabel, filelabel);
		}
	}

	extension  = strrchr(infilename, '.');

	/* Assume 3D Studio as default */
	if (!extension) extension = "3ds";
	else extension++;

	if (!stricmp(extension, "lw") || !stricmp(extension, "lwob"))
		retval = readlwfile(infilename);
	else
		retval = read3dsfile(infilename);

	if (retval)
		return 1;


	if (verbose)
		fprintf(stdout, "Merging vertices\n");

	for (i = 0; i < numObjs; i++)
		MergeVertices( &objtab[i] );

	/* calculate all vertex normals */
	if (verbose)
		fprintf(stdout, "Calculating vertex normals\n");
	for (i = 0; i < numObjs; i++)
		CalcVertexNormals( &objtab[i] );

	if (merge_tris) {
		if (verbose)
			fprintf(stdout, "Merging faces\n");
		for (i = 0; i < numObjs; i++)
			MergeFaces( &objtab[i] );
	}

	for (i = 0; i < numObjs; i++)
		CheckUncoloredFaces( &objtab[i] );

	return write_output_file( outfilename );
//	return 0;
}

/*************************************************************************
change_extension(name, ext): creates a duplicate string, containing the
given file name but with its extension changed to ext; if the file had
no extension, one is added.
ext must contain the appropriate '.' character
**************************************************************************/

char *
change_extension(char *name, char *ext)
{
	char *s;		/* temporary string pointer */
	size_t len;		/* length of the string */
	size_t extpos;		/* position where the extension is to be added */
	char *newname;

	len = extpos = 0;

	for (s = name; *s; s++) {
		if (*s == '\\' || *s == '/') {		/* account for both UNIX and DOS path separators */
			extpos = 0;			/* no extension yet, the name isn't finished */
		} else if (*s == '.') {
			extpos = len;
		}
		len++;
	}
	if (extpos == 0)
		extpos = len;

	newname = mymalloc(len+strlen(ext)+1);		/* the "+1" is for the trailing 0 */
	if (!newname) {
		fprintf(stderr, "Fatal error: insufficient memory\n");
		exit(1);
	}
	strcpy(newname, name);
	strcpy(newname+extpos, ext);
	return newname;
}


/*
 * converts a file name (fname) into a label
 */

char *
name2label( char *fname )
{
	static char buf[128];
	char *s = buf;
	char c;

	if (output_format == FORMAT_C || output_format == FORMAT_CFLOAT) {
		*s++ = 'C';
		*s++ = '3';
		*s++ = 'D';
		*s++ = '_';
	} else if (clabels)
		*s++ = '_';
	while (*fname && *fname != '.') {
		c = *fname++;
		if (c == ' ' || c == '-')
			c = '_';
		else
			c = tolower(c);
		*s++ = c;
	}
	*s++ = 0;
	return buf;
}

/*************************************************************************
write_output_file(): write out appropriate headers, and then the
object(s)
**************************************************************************/

int
write_output_file( char *outfname )
{
	int ret;
	int (*writefile)(FILE *, Object *);
	int i;
	FILE *f;
	Object *rootobj;

	f = fopen(outfname, "w");
	if (!f) {
		perror(outfname);
		return 1;
	}

	if (output_format == FORMAT_JAG) {
		fprintf(f, ";*========================================\n");
		fprintf(f, "; .JAG/.J3D format file\n");
		fprintf(f, ";*========================================\n\n");

		writefile = JAGwritefile;
	} else if (output_format == FORMAT_ANIM) {
		fprintf(f, ";*========================================\n");
		fprintf(f, "; 3D Animation Data File\n");
		fprintf(f, ";*========================================\n\n");

		writefile = N3Dwritefile;
	} else if (output_format == FORMAT_C) {
		fprintf(f, "/*========================================\n");
		fprintf(f, "  3D Library Data File\n");
		fprintf(f, " *=======================================*/\n\n");

		writefile = Cwritefile;
	} else if (output_format == FORMAT_CFLOAT) {
		fprintf(f, "/*========================================\n");
		fprintf(f, "  3D Library Data File\n");
		fprintf(f, " *=======================================*/\n\n");

		writefile = CFwritefile;
	} else {
		fprintf(f, ";*========================================\n");
		fprintf(f, "; 3D Library Data File\n");
		fprintf(f, ";*========================================\n\n");

		writefile = N3Dwritefile;
	}
	if (output_format == FORMAT_C) {
		fprintf(f, "#include \"c3d.h\"\n");
	} else if (output_format == FORMAT_CFLOAT) {
		fprintf(f, "#define USE_FLOAT\n");
		fprintf(f, "#include \"c3d.h\"\n");
	} else {
		if (outputheader) {
			fprintf(f, "\n\t.include\t'jaguar.inc'\n\n");
			if (usedataseg)
				fprintf(f, "\t.data\n");
		}
		fprintf(f, "\t.globl\t%sdata\n",defaultlabel);
		fprintf(f, "%sdata:\n", defaultlabel);
	}

	if (animflag && (output_format == FORMAT_N3D || output_format == FORMAT_ANIM)) {
		rootobj = FixObjectLists();
		fprintf(f, "\t.dc.l\t.%s\t; pointer to root object\n", name2label(rootobj->name));
		for (i = 0; i < numObjs; i++) {
			Object *obj;

			/* remember down below that name2label uses a static buffer;
			   don't try to optimize the calls to it
			 */
			fprintf(f, ".%s:\n", name2label(objtab[i].name));
			fprintf(f, "\t.dc.l\t.%s_data\n", name2label(objtab[i].name));
			fprintf(f, "\t.dc.w\t$4000, 0, 0\n");
			fprintf(f, "\t.dc.w\t0, $4000, 0\n");
			fprintf(f, "\t.dc.w\t0, 0, $4000\n");
			fprintf(f, "\t.dc.w\t0, 0, 0\n");
			obj = objtab[i].siblings;
			if (obj)
				fprintf(f, "\t.dc.l\t.%s\t; siblings\n", name2label(obj->name));
			else
				fprintf(f, "\t.dc.l\t0\t; siblings\n");
			obj = objtab[i].children;
			if (obj)
				fprintf(f, "\t.dc.l\t.%s\t; children\n", name2label(obj->name));
			else
				fprintf(f, "\t.dc.l\t0\t; children\n");
			if (objtab[i].numframes) {
				fprintf(f, "\t.dc.l\t.%s_anim\n", name2label(objtab[i].name));
			} else {
				fprintf(f, "\t.dc.l\t0\t; no animation\n");
			}
		}
	}
	for (i = 0; i < numObjs; i++) {
		ret = writefile(f, &objtab[i]);
		if (ret) return 1;
	}
	return 0;
}
