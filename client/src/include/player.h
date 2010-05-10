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
 * Player related header file. */

#ifndef PLAYER_H
#define PLAYER_H

/**
 * Contains information about the maximum level the server supports, and
 * the experience needed to reach every single level. */
typedef struct _server_level
{
	int level;
	uint32 exp[500];
} _server_level;

/** IDs of the player doll items. */
typedef enum _player_doll_enum
{
	PDOLL_ARMOUR,
	PDOLL_HELM,
	PDOLL_GIRDLE,
	PDOLL_BOOT,
	PDOLL_RHAND,
	PDOLL_LHAND,
	PDOLL_RRING,
	PDOLL_LRING,
	PDOLL_BRACER,
	PDOLL_AMULET,
	PDOLL_SKILL_ITEM,
	PDOLL_WAND,
	PDOLL_BOW,
	PDOLL_GAUNTLET,
	PDOLL_ROBE,
	PDOLL_LIGHT,

	/* Must be last element */
	PDOLL_INIT
} _player_doll_enum;

/** Player doll item position structure. */
typedef struct _player_doll_pos
{
	/** X position. */
	int xpos;

	/** Y position. */
	int ypos;
} _player_doll_pos;

extern _server_level server_level;

#endif
