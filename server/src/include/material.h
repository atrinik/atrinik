/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

/* #defines are needed by living.h, so they must be loaded early */
#ifndef MATERIAL_H
#define MATERIAL_H

#define NROFMATERIALS			13
#define NROFMATERIALS_REAL		64

#define M_NONE			0
#define M_PAPER			1
#define M_IRON			2
#define M_GLASS			4
#define M_LEATHER		8
#define M_WOOD			16
#define M_ORGANIC		32
#define M_STONE			64
#define M_CLOTH			128
#define M_ADAMANT		256
#define M_LIQUID		512
#define M_SOFT_METAL    1024
#define M_BONE			2048
#define M_ICE			4096

/* this is all stuff we want load after the system is stable from file */

#define MATERIAL_MISC           0       /* 0 */
#define M_START_PAPER			0*64+1  /* 1-64 */
#define M_START_IRON			1*64+1  /* 65 - 128 */
#define M_START_GLASS			2*64+1  /* 129 - 192 */
#define M_START_LEATHER		    3*64+1  /* 193 - 256 */
#define M_START_WOOD			4*64+1  /* 257 - 320 */
#define M_START_ORGANIC		    5*64+1  /* 321 - 384 */
#define M_START_STONE			6*64+1  /* 385 - 448 */
#define M_START_CLOTH			7*64+1  /* 449 - 512 */
#define M_START_ADAMANT		    8*64+1  /* 513 - 576 */
#define M_START_LIQUID		    9*64+1  /* 577 - 640 */
#define M_START_SOFT_METAL      10*64+1 /* 641 - 704 */
#define M_START_BONE			11*64+1 /* 705 - 768 */
#define M_START_ICE			    12*64+1 /* 769 - 832 */

typedef struct {
  	char *name;

  	sint8 save[NROFATTACKS];
} materialtype;

typedef struct _material_real_struct {
	/* name of this material */
    char *name;

	/* % value: speed of tearing when used. (used from item_condition) NOT IMPLEMENTED YET */
    int tearing;

	/* material base quality */
    int quality;

	/* unused ext. for later use */
    int ext1;

    int ext2;

    int ext3;

	/* back ref. to material type */
    int type;

	/* we can assign a default race for this material. */
    int def_race;
    /* these race have this material then exclusive */
} material_real_struct;

extern materialtype material[NROFMATERIALS];
extern material_real_struct material_real[NROFMATERIALS * NROFMATERIALS_REAL + 1];

#endif
