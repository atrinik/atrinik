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

/**
 * @file
 * Random map related variables. */

#ifndef RANDOM_MAP_H
#define RANDOM_MAP_H

#define RM_SIZE 512

/** Random map parameters. */
typedef struct
{
	char wallstyle[RM_SIZE];
	char wall_name[RM_SIZE];
	char floorstyle[RM_SIZE];
	char monsterstyle[RM_SIZE];
	char layoutstyle[RM_SIZE];
	char doorstyle[RM_SIZE];
	char decorstyle[RM_SIZE];
	char origin_map[RM_SIZE];
	char final_map[RM_SIZE];
	char exitstyle[RM_SIZE];
	char this_map[RM_SIZE];
	char dungeon_name[RM_SIZE];

	int Xsize;
	int Ysize;
	int expand2x;
	int layoutoptions1;
	int layoutoptions2;
	int layoutoptions3;
	int symmetry;
	int difficulty;
	int difficulty_given;
	int dungeon_level;
	int dungeon_depth;
	int decoroptions;
	int orientation;
	int origin_y;
	int origin_x;
	int random_seed;
	int map_layout_style;
	int symmetry_used;
	int num_monsters;
	int darkness;
	int level_increment;
} RMParms;

/**
 * @defgroup RM_LAYOUT Random map layout
 *@{*/
#define ONION_LAYOUT          1
#define MAZE_LAYOUT           2
#define SPIRAL_LAYOUT         3
#define ROGUELIKE_LAYOUT      4
#define SNAKE_LAYOUT          5
#define SQUARE_SPIRAL_LAYOUT  6
#define NROFLAYOUTS           6
/*@}*/

/**
 * @defgroup OPT_xxx Random map layout options.
 *@{*/

/** Random option. */
#define OPT_RANDOM     0
/** Centered. */
#define OPT_CENTERED   1
/** Linear doors (default is nonlinear). */
#define OPT_LINEAR     2
/** Bottom-centered. */
#define OPT_BOTTOM_C   4
/** Bottom-right centered. */
#define OPT_BOTTOM_R   8
/** Irregularly/randomly spaced layers (default: regular). */
#define OPT_IRR_SPACE  16
/** No outer wall. */
#define OPT_WALL_OFF   32
/** Only walls. */
#define OPT_WALLS_ONLY 64
/**< Place walls instead of doors.  Produces broken map. */
#define OPT_NO_DOORS   256
/*@}*/

/**
 * @defgroup SYM_xxx Random map symetry
 * Symmetry definitions -- used in this file AND in @ref treasure.c, the
 * numerical values matter so don't change them.
 *@{*/

/** Random symmetry. */
#define RANDOM_SYM  0
/** No symmetry. */
#define NO_SYM      1
/** Vertical symmetry. */
#define X_SYM       2
/** Horizontal symmetry. */
#define Y_SYM       3
/** Reflection. */
#define XY_SYM      4
/*@}*/

/**
 * Macro to get a strongly centered random distribution, from 0 to x,
 * centered at x / 2. */
#define BC_RANDOM(x) ((int) ((RANDOM() % (x) + RANDOM() % (x) + RANDOM() % (x)) / 3.))

#endif
