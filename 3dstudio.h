/*
 * 3D Studio converter module for 3DSCONV.
 *
 * Copyright 1995 Atari Corporation. All Rights Reserved.
 *
 * NOTE NOTE NOTE: This file is derived from information
 * and code provided by Autodesk, Inc., and may not
 * be redistributed without their permission!
 */

/* various definitions for 3D studio */
#define MAT_MAPNAME	0xa300
#define COLOR_F		0x0010
#define COLOR_24	0x0011

#define MDATA		0x3d3d
#define MSCALE		0x0100
#define MAT_ENTRY	0xafff
#define MAT_NAME 	0xa000
#define MAT_AMBIENT	0xa010
#define MAT_DIFFUSE	0xa020
#define MAT_SPECULAR	0xa030
#define MAT_TEXMAP	0xa200

#define NAMED_OBJECT	0x4000
#define N_TRI_OBJECT	0x4100
#define POINT_ARRAY	0x4110
#define POINT_FLAG_ARRAY	0x4111
#define FACE_ARRAY	0x4120
#define MSH_MAT_GROUP	0x4130
#define SMOOTH_GROUP	0x4150
#define TEX_VERTS	0x4140
#define MSH_MATRIX	0x4160

#define N_DIRECT_LIGHT	0x4600
#define N_CAMERA	0x4700

#define KFDATA		0xb000
#define KFHDR		0xb00a
#define KFSEG		0xb008

#define OBJECT_NODE_TAG	0xb002
#define CAMERA_NODE_TAG	0xb003
#define TARGET_NODE_TAG	0xb004
#define LIGHT_NODE_TAG	0xb005
#define L_TARGET_NODE_TAG	0xb006
#define SPOTLIGHT_NODE_TAG	0xb007

#define NODE_HDR	0xb010
#define PIVOT		0xb013
#define INSTANCE_NAME	0xb011

#define POS_TRACK_TAG	0xb020
#define ROT_TRACK_TAG	0xb021

