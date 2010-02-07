/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

/**
 * @file
 * Map header file. */

#ifndef MAP_H
#define MAP_H

/** Map start X offset */
#define MAP_START_XOFF 376

/** Map start Y offset */
#define MAP_START_YOFF 143

/** Map tile position Y offset */
#define MAP_TILE_POS_YOFF 23

/** Map tile position Y offset 2 */
#define MAP_TILE_POS_YOFF2 12

/** Map tile position X offset */
#define MAP_TILE_POS_XOFF 48

/** Map tile position X offset 2 */
#define MAP_TILE_POS_XOFF2 24

/** Map tile X offset */
#define MAP_TILE_XOFF 12

/** Map tile Y offset */
#define MAP_TILE_YOFF 24

/** Multi part object tile structure */
typedef struct _multi_part_tile
{
	/** X-offset */
	int xoff;

	/** Y-offset */
	int yoff;
} _multi_part_tile;

/** Table of predefined multi arch objects.
 * mpart_id and mpart_nr in the arches are commited from server
 * to analyze the exact tile position inside a mpart object.
 *
 * The way of determinate the starting and shift points is explained
 * in the dev/multi_arch folder of the arches, where the multi arch templates &
 * masks are. */
typedef struct _multi_part_obj
{
	/** Natural xlen of the whole multi arch */
	int xlen;

	/** Same for ylen */
	int ylen;

	/** Tile */
	_multi_part_tile part[16];
} _multi_part_obj;

/** Map data structure */
typedef struct _mapdata
{
	/** Map name. */
	char name[256];

	/** Map background music. */
	char music[256];

	/** X length. */
	int xlen;

	/** Y length. */
	int ylen;

	/** Position X. */
	int posx;

	/** Position Y. */
	int posy;
} _mapdata;

/** Map cell structure. */
struct MapCell
{
	/** Faces. */
	short faces[MAXFACES];

	/** Position. */
	short pos[MAXFACES];

	/** Fog of war. */
	int fog_of_war;

	/** Flags. */
	uint8 ext[MAXFACES];

	/** Name of player on this cell. */
	char pname[MAXFACES][32];

	/** Player name color on this cell. */
	int pcolor[MAXFACES];

	/** If this is where our enemy is. */
	uint8 probe[MAXFACES];

	/** Cell darkness. */
	uint8 darkness;

	/** Height of this maptile. */
	sint16 height;

	/** How we stretch this is really 8 char for N S E W. */
	uint32 stretch;
} MapCell;

/** Map structure. */
struct Map
{
	/** Map cells. */
	struct MapCell cells[MAP_MAX_SIZE][MAP_MAX_SIZE];
} Map;

extern _mapdata MapData;

#endif
