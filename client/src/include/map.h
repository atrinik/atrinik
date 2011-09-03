/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
#define MAP_START_XOFF 386

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

/** Maximum number of layers. */
#define MAX_LAYERS 7

/** Multi part object tile structure */
typedef struct _multi_part_tile
{
	/** X-offset */
	int xoff;

	/** Y-offset */
	int yoff;
} _multi_part_tile;

/** Table of predefined multi arch objects.
 * mpart_id and mpart_nr in the arches are committed from server
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
	char name[HUGE_BUF];

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
typedef struct MapCell
{
	/** Position. */
	uint8 quick_pos[MAX_LAYERS + 1];

	/** Player name color on this cell. */
	char pcolor[MAX_LAYERS + 1][COLOR_BUF];

	/** If this is where our enemy is. */
	uint8 probe[MAX_LAYERS + 1];

	/** Cell darkness. */
	uint8 darkness;

	/** Object flags. */
	uint8 flags[MAX_LAYERS + 1];

	/** Double drawing. */
	uint8 draw_double[MAX_LAYERS + 1];

	/** Alpha value. */
	uint8 alpha[MAX_LAYERS + 1];

	/** Faces. */
	sint16 faces[MAX_LAYERS + 1];

	/** Height of this maptile. */
	sint16 height[MAX_LAYERS + 1];

	/** Zoom. */
	sint16 zoom[MAX_LAYERS + 1];

	/** Align. */
	sint16 align[MAX_LAYERS + 1];

	/** Rotate. */
	sint16 rotate[MAX_LAYERS + 1];

	/** Whether to show the object in red. */
	uint8 infravision[MAX_LAYERS + 1];

	/** How we stretch this is really 8 char for N S E W. */
	uint32 stretch;

	/** Name of player on this cell. */
	char pname[MAX_LAYERS + 1][64];
} MapCell;

/** Map structure. */
typedef struct Map
{
	/** Map cells. */
	struct MapCell cells[MAP_MAX_SIZE][MAP_MAX_SIZE];
} Map;

#endif
