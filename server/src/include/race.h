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
 * Used to link the race lists together.
 *
 * Initialized by init_races(). */
typedef struct ralnk
{
	/** Name of this race entry */
	const char *name;

	/** Number of things belonging to this race */
	int nrof;

	/** The default corpse of this race */
	struct archt *corpse;

	/** Linked object list of things belonging to this race */
	struct oblnk *member;

	/** Next race */
	struct ralnk *next;
} racelink;

/** Marks no race. */
#define RACE_TYPE_NONE 0

/**
 * This list is used for the item prefixes and not for the race list.
 *
 * Both are different lists with different meanings. */
typedef enum _race_names_enum
{
	RACE_NAME_DEFAULT,
	RACE_NAME_DWARVEN,
	RACE_NAME_ELVEN,
	RACE_NAME_GNOMISH,
	RACE_NAME_DROW,
	RACE_NAME_ORCISH,
	RACE_NAME_GOBLIN,
	RACE_NAME_KOBOLD,
	RACE_NAME_GIANT,
	RACE_NAME_TINY,
	RACE_NAME_DEMONISH,
	RACE_NAME_DRACONISH,
	RACE_NAME_OGRE,
	RACE_NAME_INIT
} _race_names_enum;

/** Races */
typedef struct _races
{
	/** Prefix name for this race */
	char *name;

	/** Race can use (wear, wield, apply...) items from this race */
	uint32 usable;
} _races;

extern struct _races item_race_table[RACE_NAME_INIT];
